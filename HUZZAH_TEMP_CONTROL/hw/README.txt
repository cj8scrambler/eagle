To generate gerber files for OSH park:
  1) open schematic in Eagle
  2) Run the “CAM Processor”
  3) Select File -> CAM Processor
  4) Choose the OSHPark_eagle[version].cam that corresponds to your version of Eagle PCB
  5) Run 'Process Job'
  6) zip oshpark_gerbers.zip *.ger *.xln
  7) Upload oshpark_gerbers.zip to OSHPark
