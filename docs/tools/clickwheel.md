# Click Wheel designer
This is a simple tool to create a click wheel footprint. It allows you to choose the pattern and the size of the wheel. The tool will generate a PNG file that you can use in Kicad. (edge cuts and copper layers).

## Designer
<Clickwheel />

## How to import it in Kicad 

 - Use the UI above to choose your pattern
 - Click on the "Download PNG" button to download the PNG file 
 - In Kicad, use the image converter, and copy the shape to clipboard, for the size, take the radius +2mm (there is a margin around the image)
 - Paste the shape in your PCB Editor ( press E to change the layer to cut/copper)
 - You might want to download the JSON file as well, in case you need to change the pattern later