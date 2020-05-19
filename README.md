# FinBERT: Pre-Trained on SEC Filings for Financial NLP Tasks
  Vinicio DeSola, Kevin Hanna, Pri Nonis

## MODEL WEIGHTS
- https://drive.google.com/drive/folders/1rcRXZhb3JLY3A_kIO8gMk8jacRyR-Ik6?usp=sharing

## PUBLICATION
- https://www.researchgate.net/publication/334974348_FinBERT_pre-trained_model_on_SEC_filings_for_financial_natural_language_tasks

## MOTIVATIONS

<b>Goal 1</b> `FinBERT-Prime_128MSL-500K+512MSL-10K vs BERT`
 - Compare mask LM prediction accurracy on technical financial sentences
 - Compare analogy on financial relationships

<b>Goal 2</b> `FinBERT-Prime_128MSL-500K vs FinBERT-Pre2K_128MSL-500K`
 - Compare mask LM prediction accuracy on financial news from 2019
 - Compare analogy on financial relationship, measure shift in understanding : risk vs climate in 1999 vs 2019

<b>Goal 3</b> `FinBERT-Prime_128MSL-500K vs FinBERT-Prime_128MSK-500K+512MSL-10K `
 - Compare mask LM prediction accuracy on *long* financial sentences

<b>Goal 4</b> `FinBERT-Combo_128MSL-250K vs FinBERT-Prime_128MSL-500K+512MSL-10K`
 - Compare mask LM prediction accuracy on financial sentences : can we get same accuracy with less training by building on original BERT weights.

## TERMINOLOGY
- *`Prime`* `Pre-trained from scratch on 2017, 2018, 2019 SEC 10K dataset`

- *`Pre2K`* `Pre-traind from scratch on 1998, 1999 SEC 10K dataset`

- *`Combo`* `Pre-trained continued from original BERT on 2017, 2018, 2019 SEC 10K dataset`

## ANALYSIS

![](figs/analysis-001.jpg)
