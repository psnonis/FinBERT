import logging
import csv
import os
from google.cloud import storage, bigquery
from collections import OrderedDict

"""
This is ugly, but it's only meant to be run once.
"""


def get_storage_client():
    dirname = os.path.dirname(__file__)
    client = storage.Client.from_service_account_json(
        dirname + "/../keys/w266-creds.json"
    )
    return client

def get_bigquery_client():
    dirname = os.path.dirname(__file__)
    client = bigquery.Client.from_service_account_json(
        dirname + "/../keys/w266-creds.json"
    )
    return client


class Codes:

    @staticmethod
    def get_cik_codes():
        dirname = os.path.dirname(__file__)
        cik_codes = []
        with open(dirname + '/../data/cik_ticker.csv', newline='') as csvfile:
            cik_reader = csv.reader(csvfile, delimiter='|', )
            next(cik_reader, None)  # skip the headers
            for row in cik_reader:
                cik_codes.append(row[0])

        return cik_codes


class Sec10kFilings:

    def __init__(self,
                 user_project='w266-kevinhanna',
                 storage_bucket="sec_raw_data",
                 cs_source_dir="sec_edgar_filings/",
                 local_tmp_dir="/tmp",
                 ):

        self.user_project=user_project
        self.storage_bucket=storage_bucket
        self.source_dir=cs_source_dir
        self.tmp_dir=local_tmp_dir

        self.storage_client = get_storage_client()
        self.cik_codes = Codes.get_cik_codes()

    def get_available_10k_filings(self, cik):
        """
        Returns the years for which there are filings for this CIK

        :param cik: CIK for the company you wish to get 10-K's for
        :return: OrderedDict key=year value=filename ordered by year
        """

        bucket = self.storage_client.bucket(self.storage_bucket, self.user_project)
        blobs = bucket.list_blobs(prefix=self.source_dir + str(cik) + "/10-K/")

        return self.__format_sort_filings(blobs)

    def fetch_10k_filing(self, filepath, local_tmp_dir=None):
        """
        :param filepath: full path to file
        :param local_tmp_dir: A temporary directory to store temporary (need to manually delete them when done)
        :return: Filename to downloaded filing
        """

        if local_tmp_dir is None:
            local_tmp_dir = self.tmp_dir

        bucket = self.storage_client.bucket(self.storage_bucket, self.user_project)
        blob = storage.Blob(filepath, bucket)

        tmp_filename = local_tmp_dir + "/" + filepath
        os.makedirs(os.path.dirname(tmp_filename), exist_ok=True)

        with open(tmp_filename, "wb") as file_obj:
            self.storage_client.download_blob_to_file(blob, file_obj)

        return tmp_filename

    @staticmethod
    def __format_sort_filings(blobs):
        """
        Returns and OrderedDict of filings passed in as blobs (google cloud list_blobs function)
        :param blobs: likely the result of bucket.list_blobs
        :return: OrderedDict key=year value=filename ordered by year based on 2 digit year in filename where > 20
                means 19xx and < 20 means 20xx (Y2K + 20 bug)
        """
        results = {}
        for blob in blobs:
            filename = blob.name
            short_year = int(filename.split('-')[-2:-1][0])
            if short_year < 20:
                year = 2000 + short_year
            else:
                year = 1900 + short_year

            results[year] = filename

        return OrderedDict(sorted(results.items()))


class Sec10k:
    def __init__(self,
                 meta_data,
                 ten_k_documents,
                 user_project='w266-kevinhanna',
                 storage_bucket="sec_raw_data",
                 # cs_destination_dir="sec_edgar_filings/",
                 ):

        self.meta_data = meta_data
        self.ten_k_documents = ten_k_documents
        self.user_project = user_project
        self.storage_bucket = storage_bucket
        # self.cs_destination_dir = cs_destination_dir

        self.bigquery_client = get_bigquery_client()
        self.storage_client = get_storage_client()

        self.__dataset_id = 'finance_embedding'
        self.__documents_table_id = '10KDocuments'
        self.__meta_table_id = '10KMeta'
        self.__bucket_name = "sec_parsed_data"

        self.__base_dest_dir = "10-K/"

    def save(self):
        # If update is needed add logic here
        self.__insert_10K()

    def __insert_10K(self):
        from datetime import datetime, timezone

        # Write documents to cloud storage
        bucket = self.storage_client.bucket(self.__bucket_name, self.user_project)

        cik = self.meta_data['cik']
        year = self.meta_data['year']
        assert cik is not None
        assert cik != ''
        assert year > 1919

        bq_document_inserts = []

        for document_obj in self.ten_k_documents:
            type = document_obj.get('type')
            sequence = document_obj.get('sequence')
            document = document_obj.get('document')
            description = document_obj.get('description')

            filepath = self.__base_dest_dir + str(cik) + "/" + str(year) + "/" + type + "_" + sequence + ".txt"

            blob = bucket.blob(filepath)
            blob.upload_from_string(document)

            bq_document_inserts.append(
                (
                    cik,
                    year,
                    type,
                    filepath,
                    description,
                )
            )

        documents_table_ref = self.bigquery_client.dataset(self.__dataset_id).table(self.__documents_table_id)
        documents_table = self.bigquery_client.get_table(documents_table_ref)
        errors = self.bigquery_client.insert_rows(documents_table, bq_document_inserts)

        if errors:
            for error in errors:
                logging.error("Error streaming logs to table_id: {}  Error:{}".format(self.__documents_table_id, error))

        if self.meta_data['accession_number']:
            accession_number = self.meta_data['accession_number']
        else:
            accession_number = 999999

        if self.meta_data['conformed_period_of_report']:
            period_of_report = datetime.strptime(self.meta_data['conformed_period_of_report'], '%Y%m%d').date()
        else:
            period_of_report = datetime.strptime(str(year)+"1231", '%Y%m%d').date()
            logging.warn("Guessing period of report for CIK: {}  year: {}".format(cik, year))

        if self.meta_data['filed_as_of_date']:
            filed_as_of_date = datetime.strptime(self.meta_data['filed_as_of_date'], '%Y%m%d').date()
        else:
            filed_as_of_date = None

        if self.meta_data['fiscal_year_end']:
            fiscal_year_end = datetime.strptime(str(year)+self.meta_data['fiscal_year_end'], '%Y%m%d').date()
        else:
            fiscal_year_end = period_of_report

        bq_meta_insert = [(
            cik,
            year,
            accession_number,
            period_of_report,
            filed_as_of_date,
            self.meta_data['company_confirmed_name'],
            self.meta_data['standard_industrial_classification'],
            self.meta_data['state_of_incorporation'],
            fiscal_year_end,
            datetime.now(timezone.utc)
        )]

        meta_table_ref = self.bigquery_client.dataset(self.__dataset_id).table(self.__meta_table_id)
        meta_table = self.bigquery_client.get_table(meta_table_ref)
        errors = self.bigquery_client.insert_rows(meta_table, bq_meta_insert)

        if errors:
            for error in errors:
                logging.error("Error streaming logs to table_id: {}  Error:{}".format(self.__meta_table_id, error))


class CorpusBuilder:

    def __init__(self):
        self.corpus_set = None
        self.bigquery_client = get_bigquery_client()
        self.storage_client = get_storage_client()

        self.__bucket_name = "sec_parsed_data"
        self.user_project = "w266-kevinhanna"

    def get_by_cik_year(self, cik, year):
        # Perform a query.
        QUERY = (
            'SELECT CIK, Year, DocumentType, DocumentURI, Description '
            'FROM `w266-kevinhanna.finance_embedding.10KDocuments` '
            'WHERE CIK = "1340282" '
            'ORDER BY Year, DocumentType ASC'
        )
        query_job = self.bigquery_client.query(QUERY)  # API request
        rows = query_job.result()  # Waits for query to finish

        self.corpus_set = rows

        return self.corpus_set
        # for row in rows:
        #     print(row.DocumentURI)

    def __iter__(self):
        return self

    def __next__(self):
        row = next(self.corpus_set)

        if not row:
            return None

        print(row.get('DocumentURI'))

        document = self.__fetch_document(row.DocumentURI)
        return document


    def fetch_document_old(self, document_uri):
        from io import BytesIO

        bucket = self.storage_client.bucket(self.__bucket_name, self.user_project)

        # bucket = self.storage_client.get_bucket(self.__bucket_name, self.user_project)
        # blob = bucket.get_blob(document_uri)
        blob = storage.Blob(document_uri, bucket)

        string_buffer = BytesIO()
        blob.download_to_file(string_buffer)

        self.storage_client.download_blob_to_file(blob, string_buffer)

        return string_buffer.getvalue()

        # document_string = blob.download_as_string()
        # return document_string

        # bucket = self.storage_client.bucket(self.storage_bucket, self.user_project)
        # blob = storage.Blob(filepath, bucket)

    def fetch_document_foo(self, document_uri):
        from io import BytesIO


        bucket = self.storage_client.bucket(self.__bucket_name, self.user_project)

        # bucket = self.storage_client.bucket(self.storage_bucket, self.user_project)
        blob = storage.Blob(document_uri, bucket)

        string_buffer = BytesIO()

        self.storage_client.download_blob_to_file(blob, string_buffer)

        return string_buffer.getvalue()

    def fetch_document(self, document_uri):
        bucket = self.storage_client.bucket(self.__bucket_name, "w266-kevinhanna")
        blob = storage.Blob(document_uri, bucket)

        return blob.download_as_string()