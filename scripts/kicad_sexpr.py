"""Small S-expression reader/writer for the KiCad files used by Rev2 tools."""
import json
import re
from pathlib import Path


class Atom(str):
    pass


def read(path):
    stack, root = [], None
    for token in re.findall(r'"(?:\\.|[^"\\])*"|[()]|[^\s()]+', Path(path).read_text()):
        if token == '(':
            node = []
            if stack:
                stack[-1].append(node)
            else:
                root = node
            stack.append(node)
        elif token == ')':
            stack.pop()
        else:
            stack[-1].append(json.loads(token) if token.startswith('"') else Atom(token))
    if stack:
        raise ValueError('unclosed expression')
    return root


def children(node, tag):
    return [x for x in node if isinstance(x, list) and x and x[0] == tag]


def child(node, tag):
    return next(iter(children(node, tag)), None)


def dumps(node, indent=0):
    if isinstance(node, Atom):
        return str(node)
    if isinstance(node, str):
        return json.dumps(node, ensure_ascii=False)
    if not any(isinstance(x, list) for x in node):
        return '(' + ' '.join(dumps(x) for x in node) + ')'
    parts, first = [], True
    for x in node:
        if isinstance(x, list):
            parts.append('\n' + '  ' * (indent + 1) + dumps(x, indent + 1))
        else:
            parts.append(('' if first else ' ') + dumps(x))
        first = False
    return '(' + ''.join(parts) + ')'


def write(path, node):
    Path(path).write_text(dumps(node) + '\n')
