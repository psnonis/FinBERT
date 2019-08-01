#!/usr/bin/env python3

def split_tfrecord(tfrecord_path, split_size):

    import tensorflow as tf

    with tf.Graph().as_default(), tf.Session() as sess:
        ds = tf.data.TFRecordDataset(tfrecord_path).batch(split_size)
        batch = ds.make_one_shot_iterator().get_next()
        part_num = 0
        while True:
            try:
                records = sess.run(batch)
                part_path = tfrecord_path + '.{:03d}'.format(part_num)
                with tf.python_io.TFRecordWriter(part_path) as writer:
                    for record in records:
                        writer.write(record)
                part_num += 1
            except tf.errors.OutOfRangeError: break

def to_chunk(kevinFile, chunkFile, minCharacterLen = 1000):

    print(f'to_chunk : {kevinFile}')

    from re import sub

    corpus = open(kevinFile, 'rt').read()
    chunks = corpus.split('\n\n')

    print(f'starting with {len(chunks)} chunks')

    chunks = [c                     for c in chunks if len(c) > minCharacterLen]

    chunks = [sub('\n',     ' ', c) for c in chunks]
    chunks = [sub('\s+',    ' ', c) for c in chunks]
    chunks = [sub('\s-+\s', ' ', c) for c in chunks]

    chunks = [c.strip()             for c in chunks]
    chunks = [c.replace('. ','.\n') for c in chunks]

    print(f'ending   with {len(chunks)} chunks')

    corpus = '\n\n'.join(chunks)

    with open(chunkFile, 'wt') as f :

        f.write(corpus)

def to_tfrec(chunkFile, tfrecFile) :

    print(f'to_tfrec : {chunkFile}')

    from os import system

   # max_predictions_per_seq : must match run_pretraining.py
   # max_seq_length          : must match run_pretraining.py

    max_seq = 128 #  6% of sentences on 2019 are longer than 64 (64x2 for the two sentences)
    max_pre = 20  # 15% of 128

    max_seq = 512
    max_pre = 80

    command = f"nohup python3 create_pretraining_data.py \
            --input_file={chunkFile} \
            --output_file={tfrecFile}-{max_seq}-{max_pre} \
            --vocab_file=vocab.txt \
            --do_lower_case=True \
            --do_whole_word_mask=True \
            --max_predictions_per_seq={max_pre} \
            --max_seq_length={max_seq} \
            --masked_lm_prob=0.15 \
            --random_seed=42 \
            --dupe_factor=5 &"

    system(command)

if __name__ == '__main__':

    from sys     import argv
    from glob    import glob
    from os.path import exists
    from os      import system

    kevins = sorted(glob((argv + ['*'])[1] + '.kevin'))

    for kevin in kevins :
        chunk  = kevin.replace('.kevin', '.chunk')

        if not exists(chunk) : to_chunk(kevin, chunk)

  # split -n 8 -d 2019.chunk 2018.chunk.

    chunks = sorted(glob((argv + ['*'])[1] + '.chunk'))

    for chunk in chunks :
        split  = chunk.replace('.chunk', '.chunk.00')

        if not exists(split) : system(f'split -n 8 -d {chunk} {chunk}.')

    chunks = sorted(glob((argv + ['*'])[1] + '.chunk.*'))

    for chunk in chunks :
        tfrec  = chunk.replace('.chunk', '.tfrecord')

        if not exists(tfrec) : to_tfrec(chunk, tfrec)


