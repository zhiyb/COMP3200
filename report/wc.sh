#!/bin/bash
pdftk report.pdf cat 6-r8 output report_s.pdf
pdftotext report_s.pdf - | wc -w
