#! /usr/bin/env python

"""
Download, unzip, and consume a Newick file.  Produce a txt file.

The dataset that this script consumes is associated with the following
publication.

Hinchliff CE, Smith SA, Allman JF, Burleigh JG, Chaudhary R, Coghill LM,
Crandall KA, Deng J, Drew BT, Gazis R, Gude K, Hibbett DS, Katz LA,
Laughinghouse IV HD, McTavish EJ, Midford PE, Owen CL, Ree RH, Rees JA, Soltis
DE, Williams T, Cranston KA (2015) Synthesis of phylogeny and taxonomy into a
comprehensive tree of life. Proceedings of the National Academy of Sciences of
the United States of America 112(41): 12764–12769.
"""

import urlparse
import argparse
import os
import sys
import requests
from clint.textui import progress, colored, puts
from Bio import Phylo

TAXONOMY_URL = 'http://files.opentreeoflife.org/ott/ott2.9/ott2.9.tgz'
EXAMPLE_URL = 'http://github.prideout.net/assets/terrainpts.bin'
SYNTHETIC_URL = 'http://files.opentreeoflife.org/trees/draftversion4.tre.gz'
DEFAULT_URL = SYNTHETIC_URL
TREE_VERSION = '0000'

parser = argparse.ArgumentParser(description=__doc__.strip())
parser.add_argument('-url',
                    default=DEFAULT_URL,
                    help="default: %(default)s")


def download_file(url, filename):
    response = requests.get(url, stream=True)
    total_length = int(response.headers.get('content-length'))
    with open(filename, 'wb') as f:
        nchunks = total_length / 1024 + 1
        iterator = response.iter_content(chunk_size=1024)
        for chunk in progress.bar(iterator, expected_size=nchunks):
            if chunk:
                f.write(chunk)
    print

if __name__ == '__main__':
    args = parser.parse_args()
    filename = urlparse.urlsplit(args.url).path.split('/')[-1]

    # Download it
    if not os.path.exists(filename):
        puts(colored.yellow('Fetching file...'))
        download_file(args.url, filename)

    # Unzip it
    if filename.endswith('.tgz'):
        tar = tarfile.open(filename, 'r:gz')
        with indent(4, quote=' >'):
            for item in tar:
                puts(item)
                tar.extract(item)
    elif filename.endswith('.gz'):
        puts(colored.red('Unzipping is not implemented.'))
    filename = os.path.splitext(filename)[0]

    # Parse it
    puts('Parsing file...')
    from Bio import Phylo
    tree = Phylo.read('draftversion4.tre', 'newick')

    # Make a dictionary
    puts('Building object graph...')
    treedict = {}
    linenos = {}
    lineno = 0
    for clade in tree.find_clades():
        linenos[clade] = lineno
        lineno = lineno + 1
        for child in clade:
            treedict[child] = clade

    # Create a simple text file
    puts('Dumping simple text file...')
    outfile = open('monolith.{}.txt'.format(TREE_VERSION), 'wt')
    for clade in tree.find_clades():
        if clade.name:
            name = ' '.join(clade.name.split('_')[:-1])
        else:
            name = '*'
        parent = treedict.get(clade, None)
        lineno = linenos.get(parent, 0)
        outfile.write('{0:7} {1}\n'.format(lineno, name))
