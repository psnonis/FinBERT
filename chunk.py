def chunkify(kevin, minCharacterLen = 1000):

    from re import sub

    corpus = open(kevin, 'rt').read()
    chunks = corpus.split('\n\n')

    print(f'starting with {len(chunks)} chunks')

    chunks = [c                     for c in chunks if len(c) > minCharacterLen]

    chunks = [sub('\n',     ' ', c) for c in chunks]
    chunks = [sub('\s+',    ' ', c) for c in chunks]
    chunks = [sub('\s-+\s', ' ', c) for c in chunks]

    chunks = [c.strip()             for c in chunks]
    chunks = [c.replace('. ','.\n') for c in chunks]

    print(f'ending   with {len(chunks)} chunks')

    return chunks

import sys

if __name__ == '__main__':
    
    year  = sys.argv[1]

    kevin = f'data/{year}.kevin'
    chunk = f'data/{year}.chunk'

    data  = chunkify(kevin)
    text = '\n\n'.join(data)

    with open(chunk, 'wt') as f :

        f.write(text)
