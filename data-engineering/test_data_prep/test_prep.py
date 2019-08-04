import csv

must_mask_filepaths = ['./test_data/Negative.csv', './test_data/Positive.csv']
must_not_mask_filepaths = []

must_mask = []
must_not_mask = []

def words_from_file(filepath):
    results = []
    with open(filepath) as csv_file:
        csv_reader = csv.reader(csv_file, delimiter=',')
        for row in csv_reader:
            results.append(row[0].lower())
    return results


def filepaths_to_list(filepaths):
    results = []
    for file in filepaths:
        results.extend(words_from_file(file))
    return results

# def mask_must_masks(text, must_mask):



must_mask = filepaths_to_list(must_mask_filepaths)

print(must_mask)





# earnings_call_raw_path = '../data/earnings_call_raw.txt'
#
# with open(earnings_call_raw_path) as csv_file:
#     csv_reader = csv.reader(csv_file, delimiter=' ')
#     for row in csv_reader:
#         if test_row(row):
#             cleaned_row = clean_row(row)
#             masked_row = mask_row(cleaned_row)
#             output.append(masked_row)