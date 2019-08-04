import logging
import sys
import os

from model import documents
from parsers import filing_parsers


def main(*args):
    ciks = documents.Codes.get_cik_codes()
    ten_k_filing = documents.Sec10kFilings()
    start_at = '825313'
    stop_at = '906113'

    do_parse = False

    for cik in ciks:

        if cik == start_at:
            do_parse = True

        if do_parse and cik == stop_at:
            do_parse = False

        if not do_parse:
            continue

        # Get a list of 10-K's
        yearly_filing_files = ten_k_filing.get_available_10k_filings(cik)

        for year, filename in yearly_filing_files.items():
            try:
                logging.info("Parsing cik: {}  year: {}".format(cik, year))
                local_tmp_dir = '/tmp'
                filepath = ten_k_filing.fetch_10k_filing(filename, local_tmp_dir=local_tmp_dir)
                parsed_filing = filing_parsers.parse_10k(filepath, cik, year)

                # Store documents
                new_10k = documents.Sec10k(parsed_filing.get('headers'), parsed_filing.get('documents'))
                new_10k.save()

                # Cleanup tmp file
                os.remove(local_tmp_dir + "/" + filename)
            # Log it and continue
            except Exception as e:
                logging.error("Unexpected error:", sys.exc_info()[0])
            except:
                logging.error("Unexpected error:", sys.exc_info()[0])



if __name__ == '__main__':
    logging.getLogger().setLevel(logging.INFO)
    main(*sys.argv)