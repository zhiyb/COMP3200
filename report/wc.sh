#!/bin/bash
pdftk report.pdf cat 5-r4 output report_s.pdf
pdftotext report_s.pdf - | wc -w
