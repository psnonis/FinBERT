import sys
from google.cloud import bigquery



def get_bq_client():
    client = bigquery.Client.from_service_account_json(
        "../keys/w266-creds.json"
    )
    return client

def query_stackoverflow():
    client = get_bq_client()
    query_job = client.query("""
        SELECT
          CONCAT(
            'https://stackoverflow.com/questions/',
            CAST(id as STRING)) as url,
          view_count
        FROM `bigquery-public-data.stackoverflow.posts_questions`
        WHERE tags like '%google-bigquery%'
        ORDER BY view_count DESC
        LIMIT 10""")

    results = query_job.result()  # Waits for job to complete.

    for row in results:
        print("{} : {} views".format(row.url, row.view_count))

def main(*args):
    query_stackoverflow()



if __name__ == '__main__':
    main(*sys.argv)
