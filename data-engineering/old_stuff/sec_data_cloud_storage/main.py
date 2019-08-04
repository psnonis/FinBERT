import csv
import os
import sys
import time
from google.cloud import storage

base_local_dir = "/home/khanna/Transfer/financial_reports/sec_edgar_filings/"
base_dest_dir = "kevin_sandbox/"

def get_cik_codes():
    cik_codes = []
    with open('../sec_data_harvest/data/cik_ticker.csv', newline='') as csvfile:
        cik_reader = csv.reader(csvfile, delimiter='|', )
        next(cik_reader, None)  # skip the headers
        for row in cik_reader:
            cik_codes.append(row[0])

    return cik_codes

def get_cl_client():
    client = storage.Client.from_service_account_json(
        "../keys/w266-creds.json"
    )
    return client


def upload_10_k():
    bucket_name = "sec_raw_data"
    user_project = 'w266-kevinhanna' # This needs to be a projec

    start_at = 6158  # To restart at a specific point

    total_time=0
    total_filings= start_at or 0

    all_cik_codes = get_cik_codes()

    for i in range(start_at, len(all_cik_codes)):
        cik = all_cik_codes[i]
        start = time.time()

        for source_file_name, destination_blob_name in  get_file_path_names(cik):

            """Uploads a file to the bucket."""
            storage_client = get_cl_client()
            # bucket = storage_client.get_bucket(bucket_name, user_project)
            bucket = storage_client.bucket(bucket_name, user_project)

            blob = bucket.blob(base_dest_dir + str(cik) + "/10-K/" + destination_blob_name)

            blob.upload_from_filename(source_file_name)

        end = time.time()

        total_filings += 1
        total_time += end - start
        avg_time = total_time / (total_filings - start_at)
        eta_minutes = (avg_time / 60) * (len(all_cik_codes) - total_filings)

        print("Done Uploading CIK: '{0}'  {1} of {2}.  ETA {3:0.0f} minutes.".format(cik, total_filings, len(all_cik_codes), eta_minutes))


def get_file_path_names(cik):
    from os import listdir
    from os.path import isfile, join

    global base_local_dir

    # TODO move this somewhere better
    path = base_local_dir + str(cik) + "/10-K/"

    if os.path.isdir(path):
        src_dest = [(join(path, f), f) for f in listdir(path) if isfile(join(path, f))]
    else:
        return []

    return src_dest

def main(*args):
    upload_10_k()



if __name__ == '__main__':
    main(*sys.argv)
