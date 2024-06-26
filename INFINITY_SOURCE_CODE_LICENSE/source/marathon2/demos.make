DemosResource= :demos:demos.resource
DemosDemoResource= :demos.demo:demos.resource

Demos �� {DemoFiles}
	Set Echo 0
	set id 128
	delete -i "{DemosResource}"  # to make sure that any old demos are gone
	for fileName in {DemoFiles}
		echo Adding demo from "{fileName}"...
		Echo "Read 'film' (DEMO_ID, DEMO_NAME) DEMO_FILE;" | Rez -append -define DEMO_ID="{id}" -define DEMO_NAME=�""{fileName}"�" -define DEMO_FILE=�""{fileName}"�" -c 'RSED' -t 'RSRC' -o "{DemosResource}"
		set id `Evaluate ({id} + 1)`
	end

Demos.demo �� {DemoFiles}
	Set Echo 0
	set id 128
	delete -i "{DemosDemoResource}"  # to make sure that any old demos are gone
	for fileName in {DemoFiles}
		echo Adding demo from "{fileName}"...
		Echo "Read 'film' (DEMO_ID, DEMO_NAME) DEMO_FILE;" | Rez -append -define DEMO_ID="{id}" -define DEMO_NAME=�""{fileName}"�" -define DEMO_FILE=�""{fileName}"�" -c 'RSED' -t 'RSRC' -o "{DemosDemoResource}"
		set id `Evaluate ({id} + 1)`
	end
