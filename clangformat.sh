#!/usr/bin/sh
git archive HEAD | tar -t | egrep '[.]cpp$|[.]h$' | xargs clang-format -i
git archive HEAD | tar -t | egrep '[.]cpp$|[.]h$' | xargs dos2unix
