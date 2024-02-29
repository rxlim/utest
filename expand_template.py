import argparse
import pathlib


parser = argparse.ArgumentParser()
parser.add_argument("template", type=pathlib.Path)
parser.add_argument("replacements", nargs="+")
parser.add_argument("--out", "-o", type=pathlib.Path)

args = parser.parse_args()

with args.template.open("r") as tmpl:
    template = tmpl.read()

result = template
for r in args.replacements:
    parts = r.split("=")
    if len(parts) == 2:
        k, v = r.split("=")
        result = result.replace("@{" + k + "}", v)

with args.out.open("w") as out:
    out.write(result)
