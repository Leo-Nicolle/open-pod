#!/usr/bin/env python3
"""
High-performance font converter for embedded displays
Converts TrueType/OpenType fonts to optimized C++ arrays with anti-aliasing
"""

import freetype
import numpy as np
from PIL import Image
import os
import sys

class FontConverter:
    def __init__(self, font_path, size, name):
        self.face = freetype.Face(font_path)
        self.face.set_pixel_sizes(0, size)
        self.size = size
        self.name = name
        self.glyphs = []
        
    def convert_font(self, first_char=32, last_char=126):
        """Convert font to optimized format"""
        
        # Get font metrics
        self.face.load_char('A', freetype.FT_LOAD_RENDER)
        self.baseline = self.face.glyph.bitmap_top
        self.line_height = self.face.size.height >> 6
        
        # Process each character
        for char_code in range(first_char, last_char + 1):
            self.face.load_char(chr(char_code), 
                              freetype.FT_LOAD_RENDER | 
                              freetype.FT_LOAD_TARGET_NORMAL)
            
            glyph = self.face.glyph
            bitmap = glyph.bitmap
            
            if bitmap.width == 0 or bitmap.rows == 0:
                # Empty glyph (like space)
                glyph_data = {
                    'char': chr(char_code),
                    'width': 0,
                    'height': 0,
                    'xOffset': 0,
                    'yOffset': 0,
                    'xAdvance': glyph.advance.x >> 6,
                    'alphaData': []
                }
            else:
                # Extract alpha channel with proper anti-aliasing
                if bitmap.pixel_mode == freetype.FT_PIXEL_MODE_GRAY:
                    alpha_data = np.array(bitmap.buffer, dtype=np.uint8)
                    alpha_data = alpha_data.reshape(bitmap.rows, bitmap.width)
                else:
                    # Convert mono to grayscale
                    alpha_data = self._unpack_mono_bitmap(bitmap)
                
                glyph_data = {
                    'char': chr(char_code),
                    'width': bitmap.width,
                    'height': bitmap.rows,
                    'xOffset': glyph.bitmap_left,
                    'yOffset': self.baseline - glyph.bitmap_top,
                    'xAdvance': glyph.advance.x >> 6,
                    'alphaData': alpha_data.flatten().tolist()
                }
            
            self.glyphs.append(glyph_data)
    
    def _unpack_mono_bitmap(self, bitmap):
        """Unpack monochrome bitmap to grayscale"""
        data = np.array(bitmap.buffer, dtype=np.uint8)
        output = np.zeros((bitmap.rows, bitmap.width), dtype=np.uint8)
        
        for y in range(bitmap.rows):
            for x in range(bitmap.width):
                byte_idx = y * bitmap.pitch + x // 8
                bit_idx = 7 - (x % 8)
                if data[byte_idx] & (1 << bit_idx):
                    output[y, x] = 255
                    
        return output
    
    def generate_cpp_header(self, output_path):
        """Generate C++ header file"""
        with open(output_path, 'w') as f:
            f.write(f"// Auto-generated font data for {self.name}\n")
            f.write(f"// Font size: {self.size}pt\n\n")
            f.write("#pragma once\n")
            f.write("#include <stdint.h>\n\n")
            
            # Write alpha data arrays
            for i, glyph in enumerate(self.glyphs):
                if glyph['alphaData']:
                    f.write(f"// Character '{glyph['char']}' ({ord(glyph['char'])})\n")
                    f.write(f"const uint8_t {self.name}_alpha_{ord(glyph['char'])}[] = {{\n")
                    
                    # Write data in rows
                    alpha = glyph['alphaData']
                    for row in range(glyph['height']):
                        f.write("  ")
                        row_start = row * glyph['width']
                        row_end = row_start + glyph['width']
                        row_data = alpha[row_start:row_end]
                        f.write(", ".join(f"0x{b:02X}" for b in row_data))
                        if row < glyph['height'] - 1:
                            f.write(",")
                        f.write("\n")
                    f.write("};\n\n")
            
            # Write glyph array
            f.write(f"const FastGlyph {self.name}_glyphs[] = {{\n")
            
            for glyph in self.glyphs:
                if glyph['alphaData']:
                    alpha_ptr = f"{self.name}_alpha_{ord(glyph['char'])}"
                else:
                    alpha_ptr = "nullptr"
                
                f.write(f"  {{ {glyph['width']}, {glyph['height']}, "
                       f"{glyph['xOffset']}, {glyph['yOffset']}, "
                       f"{glyph['xAdvance']}, {alpha_ptr}, nullptr }}, "
                       f"// '{glyph['char']}'\n")
            
            f.write("};\n\n")
            
            # Write font structure
            f.write(f"const FastFont {self.name} = {{\n")
            f.write(f"  {self.name}_glyphs,\n")
            f.write(f"  32,  // first char\n")
            f.write(f"  126, // last char\n")
            f.write(f"  {self.line_height}, // line height\n")
            f.write(f"  {self.baseline}, // baseline\n")
            f.write(f"  false, // hasKerning\n")
            f.write(f"  nullptr // kerningTable\n")
            f.write("};\n")
    
    def generate_preview(self, output_path):
        """Generate preview image of the font"""
        # Create image showing all characters
        chars_per_row = 16
        rows = (len(self.glyphs) + chars_per_row - 1) // chars_per_row
        
        cell_width = max(g['xAdvance'] for g in self.glyphs) + 4
        cell_height = self.line_height + 4
        
        img_width = cell_width * chars_per_row
        img_height = cell_height * rows
        
        img = Image.new('RGB', (img_width, img_height), color='white')
        pixels = img.load()
        
        for i, glyph in enumerate(self.glyphs):
            if not glyph['alphaData']:
                continue
                
            row = i // chars_per_row
            col = i % chars_per_row
            
            x_base = col * cell_width + 2
            y_base = row * cell_height + self.baseline + 2
            
            # Draw glyph
            alpha = np.array(glyph['alphaData']).reshape(glyph['height'], glyph['width'])
            
            for y in range(glyph['height']):
                for x in range(glyph['width']):
                    px_x = x_base + glyph['xOffset'] + x
                    px_y = y_base + glyph['yOffset'] + y
                    
                    if 0 <= px_x < img_width and 0 <= px_y < img_height:
                        gray = 255 - alpha[y, x]
                        pixels[px_x, px_y] = (gray, gray, gray)
        
        img.save(output_path)
        print(f"Preview saved to {output_path}")

def convert_ibm_plex_sans():
    """Convert IBM Plex Sans in multiple sizes"""
    
    # Download IBM Plex Sans if not present
    font_urls = {
        'IBMPlexSans-Regular.ttf': 'https://github.com/IBM/plex/raw/master/packages/plex-sans/fonts/complete/ttf/IBMPlexSans-Regular.ttf',
        'IBMPlexSans-Bold.ttf': 'https://github.com/IBM/plex/raw/master/packages/plex-sans/fonts/complete/ttf/IBMPlexSans-Bold.ttf'
    }
    
    # Convert different sizes
    conversions = [
        ('IBMPlexSans-Regular.ttf', 12, 'IBMPlexSans12'),
        ('IBMPlexSans-Regular.ttf', 16, 'IBMPlexSans16'),
        ('IBMPlexSans-Bold.ttf', 16, 'IBMPlexSans16Bold'),
        ('IBMPlexSans-Regular.ttf', 20, 'IBMPlexSans20'),
    ]
    
    for font_file, size, name in conversions:
        if not os.path.exists(font_file):
            print(f"Downloading {font_file}...")
            import urllib.request
            urllib.request.urlretrieve(font_urls[font_file], font_file)
        
        print(f"Converting {font_file} at {size}pt...")
        converter = FontConverter(font_file, size, name)
        converter.convert_font()
        converter.generate_cpp_header(f"{name}.h")
        converter.generate_preview(f"{name}_preview.png")
        
        # Print statistics
        total_bytes = sum(len(g['alphaData']) for g in converter.glyphs)
        print(f"  Total size: {total_bytes:,} bytes")
        print(f"  Average glyph: {total_bytes // len(converter.glyphs)} bytes")

if __name__ == "__main__":
    convert_ibm_plex_sans()