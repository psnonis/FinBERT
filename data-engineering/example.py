import logging
import traceback
import sys
import os

from model import documents
from parsers import filing_parsers

def main(*args):
    ciks = documents.Codes.get_cik_codes()
    ten_k_filing = documents.Sec10kFilings()

    cik = '60086'
    year = 1994
    # Get a list of 10-K's (just grabbing the first CIK)
    yearly_filing_files = ten_k_filing.get_available_10k_filings(cik)

    # Can iterate though filings
    # for year, filename in filings.items():
    #     print("year: {}  filename: {}".format(year, filename))
    #     print("tmp_file: {}".format(ten_k.fetch_10k_filing(filename)))

    try:
        parsed_filing = filing_parsers.parse_10k(ten_k_filing.fetch_10k_filing(yearly_filing_files[year]), cik, year)
    except Exception as e:
        logging.error("Unexpected error:", sys.exc_info()[0])
    except:
        logging.error("Unexpected error:", sys.exc_info()[0])

    # # Store documents
    # new_10k = documents.Sec10k(parsed_filing.get('headers'), parsed_filing.get('documents'))
    # new_10k.save()
    #
    #
    # # Print Headers:
    # headers = parsed_filing.get('headers')
    #
    # for key, value in headers.items():
    #     print("{}: {}".format(key, value))
    #
    # # Write Documents
    # ten_k_documents = parsed_filing.get('documents')
    #
    # for document_obj in ten_k_documents:
    #
    #     type = document_obj.get('type')
    #     sequence = document_obj.get('sequence')
    #     document = document_obj.get('document')
    #
    #     filepath = "/tmp/parsed/" + cik + "/" + str(year) + "/" + type + "_" + sequence + ".txt"
    #     print("File: {}".format(filepath))
    #
    #     os.makedirs(os.path.dirname(filepath), exist_ok=True)
    #
    #     with open(filepath, "w") as file_obj:
    #         file_obj.write(document)

    # # Put the file somewhere, this will be BigQuery or Cloud Storage
    # foo_file = "/tmp/foo.txt"
    # with open(foo_file, "w") as file_obj:
    #     file_obj.write(file_string)

if __name__ == '__main__':
    main(*sys.argv)