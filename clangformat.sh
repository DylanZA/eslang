#!/usr/bin/sh
git archive HEAD | tar -t | egrep '[.]cpp$|[.]h$' | xargs clang-format -i
