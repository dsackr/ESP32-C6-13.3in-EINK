#!/usr/bin/env python3
"""
E-Paper Test Pattern Generator
Creates test patterns to verify display functionality
Resolution: 1600 x 1200 pixels
"""

import struct
import os

WIDTH = 1600
HEIGHT = 1200
BYTES_PER_ROW = WIDTH // 8

def create_test_pattern(pattern_type, output_path):
    """Create a test pattern binary file"""
    
    print(f"Generating {pattern_type} test pattern...")
    output_bytes = bytearray()
    
    if pattern_type == "checkerboard":
        # Alternating 8x8 black and white squares
        for y in range(HEIGHT):
            for x in range(0, WIDTH, 8):
                # Determine if this 8-pixel block should be black or white
                block_x = (x // 8) % 2
                block_y = (y // 8) % 2
                is_black = (block_x + block_y) % 2 == 0
                
                byte = 0xFF if is_black else 0x00
                output_bytes.append(byte)
    
    elif pattern_type == "stripes_horizontal":
        # Horizontal stripes (every 10 rows alternates)
        for y in range(HEIGHT):
            is_black = (y // 10) % 2 == 0
            byte = 0xFF if is_black else 0x00
            for x in range(BYTES_PER_ROW):
                output_bytes.append(byte)
    
    elif pattern_type == "stripes_vertical":
        # Vertical stripes (every 10 pixels alternates)
        for y in range(HEIGHT):
            for x in range(0, WIDTH, 8):
                # Create pattern: 10 pixels black, 10 pixels white
                pixel_group = (x // 10) % 2
                byte = 0xFF if pixel_group == 0 else 0x00
                output_bytes.append(byte)
    
    elif pattern_type == "gradient":
        # Gradient from white to black (dithered)
        for y in range(HEIGHT):
            threshold = int((y / HEIGHT) * 255)
            for x in range(0, WIDTH, 8):
                byte = 0
                for bit in range(8):
                    # Dither pattern
                    pixel_x = x + bit
                    pattern_val = ((pixel_x * 17) + (y * 13)) % 256
                    if pattern_val < threshold:
                        byte |= (1 << (7 - bit))
                output_bytes.append(byte)
    
    elif pattern_type == "border":
        # White with black border
        border_thickness = 20
        for y in range(HEIGHT):
            for x in range(0, WIDTH, 8):
                byte = 0x00  # Default white
                
                # Check if we're in border region
                is_border = (y < border_thickness or 
                           y >= HEIGHT - border_thickness or
                           x < border_thickness or
                           x >= WIDTH - border_thickness)
                
                if is_border:
                    byte = 0xFF  # Black
                
                output_bytes.append(byte)
    
    elif pattern_type == "grid":
        # Grid pattern with 100x100 pixel cells
        grid_size = 100
        line_thickness = 2
        
        for y in range(HEIGHT):
            for x in range(0, WIDTH, 8):
                byte = 0
                for bit in range(8):
                    pixel_x = x + bit
                    
                    # Check if on grid line
                    on_h_line = (y % grid_size) < line_thickness
                    on_v_line = (pixel_x % grid_size) < line_thickness
                    
                    if on_h_line or on_v_line:
                        byte |= (1 << (7 - bit))
                
                output_bytes.append(byte)
    
    elif pattern_type == "text":
        # Large text pattern (simple block letters)
        output_bytes = bytearray(BYTES_PER_ROW * HEIGHT)
        
        # Draw "TEST" in large block letters (simplified)
        # This is a very basic implementation
        start_y = HEIGHT // 2 - 50
        start_x = WIDTH // 2 - 200
        
        # Just make a few large blocks as "text"
        for y in range(start_y, start_y + 100):
            for x in range(start_x, start_x + 400):
                if 0 <= y < HEIGHT and 0 <= x < WIDTH:
                    byte_idx = y * BYTES_PER_ROW + (x // 8)
                    bit_idx = 7 - (x % 8)
                    
                    # Simple pattern for "TEST"
                    if (x - start_x) % 100 < 80:
                        output_bytes[byte_idx] |= (1 << bit_idx)
    
    elif pattern_type == "white":
        # All white
        output_bytes = bytearray([0x00] * (BYTES_PER_ROW * HEIGHT))
    
    elif pattern_type == "black":
        # All black
        output_bytes = bytearray([0xFF] * (BYTES_PER_ROW * HEIGHT))
    
    elif pattern_type == "split":
        # Half black, half white (vertical split)
        for y in range(HEIGHT):
            for x in range(0, WIDTH, 8):
                byte = 0xFF if x < WIDTH // 2 else 0x00
                output_bytes.append(byte)
    
    else:
        print(f"Unknown pattern: {pattern_type}")
        return False
    
    # Verify size
    expected_size = BYTES_PER_ROW * HEIGHT
    if len(output_bytes) != expected_size:
        print(f"ERROR: Size mismatch! Expected {expected_size}, got {len(output_bytes)}")
        return False
    
    # Write to file
    with open(output_path, 'wb') as f:
        f.write(output_bytes)
    
    print(f"✓ Created {output_path} ({len(output_bytes)} bytes)")
    return True

def main():
    # Create test_patterns directory
    output_dir = "test_patterns"
    os.makedirs(output_dir, exist_ok=True)
    
    patterns = [
        "white",
        "black",
        "checkerboard",
        "stripes_horizontal",
        "stripes_vertical",
        "gradient",
        "border",
        "grid",
        "split",
        "text"
    ]
    
    print("="*50)
    print("E-Paper Test Pattern Generator")
    print("="*50)
    print(f"Creating {len(patterns)} test patterns...\n")
    
    for pattern in patterns:
        output_path = os.path.join(output_dir, f"{pattern}.bin")
        create_test_pattern(pattern, output_path)
    
    print("\n" + "="*50)
    print(f"✓ All patterns created in '{output_dir}/' directory")
    print("="*50)
    print("\nRecommended testing order:")
    print("1. white.bin      - Verify display clears properly")
    print("2. black.bin      - Verify full black display")
    print("3. split.bin      - Verify both ICs work (M/S areas)")
    print("4. checkerboard.bin - Verify pixel alignment")
    print("5. border.bin     - Verify display boundaries")
    print("6. grid.bin       - Verify even display across screen")
    print("\nUpload these to your photo frame to test functionality!")

if __name__ == "__main__":
    main()
