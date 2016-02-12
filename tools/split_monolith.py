#! /usr/bin/env python

"""
Parse a monolith and split it into a fast-loading piece and a slow-loading
piece.
"""

from clint.textui import progress, colored, puts

TREE_VERSION = '0000'
PRIMATES_ID = '17f43f'
HUMANS_ID = '17f498'
ROOT_ID = '000000'
MAX_DEPTH = 4

primates = set([PRIMATES_ID])
remaining_ids = set()
tree = {}

def gather_ancestors(result, clade_id):
    result.add(clade_id)
    parent_id = tree[clade_id][0]
    if parent_id != ROOT_ID:
        gather_ancestors(result, parent_id)

def gather_descendants(result, clade_id):
    result.add(clade_id)
    children = tree[clade_id][2]
    for child in children:
        gather_descendants(result, child)

def dump_tree(outfile, clade_id, depth):
    remaining_ids.discard(clade_id)
    parent_id, name, children = tree[clade_id]
    outfile.write(' '.join([clade_id, parent_id, name]))
    outfile.write('\n')
    n = 1
    primate = clade_id in primates
    if depth < MAX_DEPTH or primate:
        for child in children:
            n = n + dump_tree(outfile, child, depth + 1)
    return n

if __name__ == '__main__':

    puts('Parsing monolith...')
    infile = open('monolith.{}.txt'.format(TREE_VERSION), 'rt')
    for line in progress.bar(infile, expected_size=2655768):
        tokens = line.split()
        clade_id = tokens[0]
        parent_id = tokens[1]
        name = ' '.join(tokens[2:])
        tree[clade_id] = (parent_id, name, [])

    puts('Building graph...')
    remaining_ids = set(tree.keys())
    remaining_ids.remove(ROOT_ID)
    for clade_id in remaining_ids:
        parent_id = tree[clade_id][0]
        tree[parent_id][2].append(clade_id)

    puts('Gathering primates...')
    gather_ancestors(primates, PRIMATES_ID)
    gather_descendants(primates, PRIMATES_ID)

    puts('Dumping part A...')
    outfile = open('monolith.{}.a.txt'.format(TREE_VERSION), 'wt')
    nnodes = dump_tree(outfile, '000000', 0)
    print nnodes, 'nodes.'

    puts('Dumping part B...')
    outfile = open('monolith.{}.b.txt'.format(TREE_VERSION), 'wt')
    for clade_id in remaining_ids:
        parent_id, name, children = tree[clade_id]
        outfile.write(' '.join([clade_id, parent_id, name]))
        outfile.write('\n')
    print len(remaining_ids), 'nodes.'
