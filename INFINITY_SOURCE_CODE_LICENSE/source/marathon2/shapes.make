### SHAPES.MAKE
### Saturday, August 27, 1994 6:24:26 PM

Shapes= Shapes

{Shapes} �� {RawShapeFiles}
	For NewerFile In {NewerDeps}
		Echo "include FILENAME;" | Rez -append -o {Shapes}.rsrc -d `Quote "FILENAME=�"{NewerFile}�""`
	End
	SetFile {Shapes}.rsrc -c RSED -t rsrc
	:extract:shapeextract {Shapes}.rsrc {Shapes}
	SetFile {Shapes} -c 52.4 -t shp2
