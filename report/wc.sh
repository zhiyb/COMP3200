#!/bin/bash
pdftk report.pdf cat 6-r9 output report_s.pdf
pdftotext report_s.pdf - | wc -w
