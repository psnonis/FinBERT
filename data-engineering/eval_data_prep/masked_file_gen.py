import csv
import string
import numpy as np

financial_keywords_path = '../data/financial_keywords.csv'
earnings_call_raw_path = '../data/earnings_call_raw.txt'
eval_data_path = '../data/eval_data.txt'

financial_keywords = []
min_paragraph_word_length = 50
mask_ratio = .1
mask_token = '[MASK]'

output = []

def test_row(row):
    return len(row) > min_paragraph_word_length-1 and any(elem in row for elem in financial_keywords)


def clean_row(row):
    new_row = []
    for word_pre in row:
        word = word_pre.lower()
        if word[-1] in string.punctuation:
            new_row.append(word[0:-1])
            new_row.append(word[-1])
        else:
            new_row.append(word)

    return new_row

def mask_row(row):
    new_row = []
    for word in row:
        if np.random.rand() > mask_ratio or word in financial_keywords or len(word) == 1:
            if word == '.':
                new_row.append('. [SEP]')
            else:
                new_row.append(word)
        else:
            new_row.append(mask_token)

    return new_row

with open(financial_keywords_path) as csv_file:
    csv_reader = csv.reader(csv_file, delimiter=',')
    for row in csv_reader:
        financial_keywords.append(row[0])
    print(financial_keywords)

with open(earnings_call_raw_path) as csv_file:
    csv_reader = csv.reader(csv_file, delimiter=' ')
    for row in csv_reader:
        if test_row(row):
            cleaned_row = clean_row(row)
            masked_row = mask_row(cleaned_row)
            output.append(masked_row)

with open(eval_data_path, 'w') as output_file:
    for row in output:
        output_file.write('[CLS] ')
        output_file.write(' '.join(row))
        output_file.write('\n\n')

print('done')

