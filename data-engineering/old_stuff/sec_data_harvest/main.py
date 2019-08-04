import csv
import sys
import time

from sec_edgar_downloader import Downloader



def get_cik_codes():
    cik_codes = []
    with open('./data/cik_ticker.csv', newline='') as csvfile:
        cik_reader = csv.reader(csvfile, delimiter='|', )
        next(cik_reader, None)  # skip the headers
        for row in cik_reader:
            cik_codes.append(row[0])

    return cik_codes


def fetch_statements(cik, downloader):
    downloader.get_10k_filings(cik)


def main(*args):
    start_at = 13139  # To restart at a specific point

    total_time = 0
    total_filings = start_at or 0
    edgar_dl = Downloader("/home/khanna/Transfer/financial_reports")

    all_cik_codes = get_cik_codes()

    for i in range(start_at, len(all_cik_codes)):
        cik = all_cik_codes[i]

        start = time.time()
        fetch_statements(cik, edgar_dl)
        end = time.time()

        total_filings += 1
        total_time += end-start
        avg_time = total_time/(total_filings - start_at)
        eta_minutes = (avg_time/60) * (len(all_cik_codes) - total_filings)

        print("Done Fetching CIK: '{0}'  {1} of {2}.  ETA {3:0.0f} minutes.".format(cik, total_filings, len(all_cik_codes), eta_minutes))



if __name__ == '__main__':
    main(*sys.argv)
