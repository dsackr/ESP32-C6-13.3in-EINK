#!/usr/bin/env python3
"""
E-Paper Image Converter
Converts images to raw binary format for Waveshare 13.3" e-Paper display
Resolution: 1600 x 1200 pixels
Format: 1-bit black/white, MSB first
Output: .bin file (240,000 bytes)
"""

from PIL import Image, ImageEnhance
import sys
import os
import argparse

def convert_to_bin(input_path, output_path, threshold=128, contrast=1.0, brightness=1.0, invert=False, dither=False):
    """
    Convert an image to e-paper binary format
    
    Args:
        input_path: Path to input image (JPG, PNG, etc.)
        output_path: Path to output .bin file
        threshold: Black/white threshold (0-255, default 128)
        contrast: Contrast adjustment (1.0 = no change)
        brightness: Brightness adjustment (1.0 = no change)
        invert: Invert black and white
        dither: Use dithering for better gradients
    """
    
    try:
        # Open image
        print(f"Opening {input_path}...")
        img = Image.open(input_path)
        print(f"Original size: {img.size}")
        
        # Resize to 1600x1200 with high-quality resampling
        print("Resizing to 1600x1200...")
        img = img.resize((1600, 1200), Image.LANCZOS)
        
        # Convert to grayscale
        print("Converting to grayscale...")
        img = img.convert('L')
        
        # Adjust contrast if specified
        if contrast != 1.0:
            print(f"Adjusting contrast: {contrast}x")
            enhancer = ImageEnhance.Contrast(img)
            img = enhancer.enhance(contrast)
        
        # Adjust brightness if specified
        if brightness != 1.0:
            print(f"Adjusting brightness: {brightness}x")
            enhancer = ImageEnhance.Brightness(img)
            img = enhancer.enhance(brightness)
        
        # Convert to black and white
        print(f"Converting to B&W (threshold: {threshold})...")
        if dither:
            # Use Floyd-Steinberg dithering
            img = img.convert('1')
        else:
            # Simple threshold
            img = img.point(lambda x: 0 if x < threshold else 255, '1')
        
        # Invert if requested
        if invert:
            print("Inverting colors...")
            img = Image.eval(img, lambda x: 255 - x)
        
        # Convert to binary format
        print("Converting to binary format...")
        width, height = img.size
        output_bytes = bytearray()
        
        # Process row by row, 8 pixels per byte
        for y in range(height):
            for x in range(0, width, 8):
                byte = 0
                for bit in range(8):
                    if x + bit < width:
                        pixel = img.getpixel((x + bit, y))
                        # Black pixel = 1, White pixel = 0
                        if pixel == 0:
                            byte |= (1 << (7 - bit))
                output_bytes.append(byte)
            
            # Progress indicator
            if y % 100 == 0:
                print(f"Progress: {y}/{height} rows")
        
        # Verify size
        expected_size = (width * height) // 8
        actual_size = len(output_bytes)
        
        if actual_size != expected_size:
            print(f"WARNING: Size mismatch! Expected {expected_size}, got {actual_size}")
        
        # Write to file
        print(f"Writing to {output_path}...")
        with open(output_path, 'wb') as f:
            f.write(output_bytes)
        
        # Success!
        print("\n" + "="*50)
        print("✓ Conversion successful!")
        print(f"  Input:  {input_path}")
        print(f"  Output: {output_path}")
        print(f"  Size:   {len(output_bytes):,} bytes")
        print("="*50)
        
        # Save preview image
        preview_path = output_path.replace('.bin', '_preview.png')
        img.save(preview_path)
        print(f"Preview saved: {preview_path}")
        
        return True
        
    except FileNotFoundError:
        print(f"ERROR: File not found: {input_path}")
        return False
    except Exception as e:
        print(f"ERROR: {str(e)}")
        return False

def batch_convert(input_dir, output_dir, **kwargs):
    """Convert all images in a directory"""
    
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    # Supported formats
    extensions = ('.jpg', '.jpeg', '.png', '.bmp', '.gif', '.tiff')
    
    # Find all images
    images = []
    for root, dirs, files in os.walk(input_dir):
        for file in files:
            if file.lower().endswith(extensions):
                images.append(os.path.join(root, file))
    
    if not images:
        print(f"No images found in {input_dir}")
        return
    
    print(f"Found {len(images)} images to convert")
    print("="*50)
    
    # Convert each image
    success_count = 0
    for i, img_path in enumerate(images, 1):
        print(f"\n[{i}/{len(images)}] Converting {os.path.basename(img_path)}...")
        
        # Generate output filename
        base_name = os.path.splitext(os.path.basename(img_path))[0]
        output_path = os.path.join(output_dir, f"{base_name}.bin")
        
        if convert_to_bin(img_path, output_path, **kwargs):
            success_count += 1
    
    print("\n" + "="*50)
    print(f"Batch conversion complete: {success_count}/{len(images)} successful")
    print("="*50)

def main():
    parser = argparse.ArgumentParser(
        description='Convert images to e-paper binary format',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic conversion
  python convert_image.py photo.jpg photo.bin
  
  # Adjust threshold for darker/lighter output
  python convert_image.py photo.jpg photo.bin --threshold 100
  
  # Increase contrast and brightness
  python convert_image.py photo.jpg photo.bin --contrast 1.5 --brightness 1.2
  
  # Use dithering for better gradients
  python convert_image.py photo.jpg photo.bin --dither
  
  # Invert colors (black becomes white)
  python convert_image.py photo.jpg photo.bin --invert
  
  # Batch convert all images in a folder
  python convert_image.py input_folder/ output_folder/ --batch
        """
    )
    
    parser.add_argument('input', help='Input image file or directory (for batch mode)')
    parser.add_argument('output', help='Output .bin file or directory (for batch mode)')
    parser.add_argument('--threshold', type=int, default=128,
                       help='Black/white threshold (0-255, default: 128)')
    parser.add_argument('--contrast', type=float, default=1.0,
                       help='Contrast multiplier (default: 1.0)')
    parser.add_argument('--brightness', type=float, default=1.0,
                       help='Brightness multiplier (default: 1.0)')
    parser.add_argument('--invert', action='store_true',
                       help='Invert black and white')
    parser.add_argument('--dither', action='store_true',
                       help='Use Floyd-Steinberg dithering')
    parser.add_argument('--batch', action='store_true',
                       help='Batch convert all images in input directory')
    
    args = parser.parse_args()
    
    # Check if PIL is installed
    try:
        from PIL import Image
    except ImportError:
        print("ERROR: PIL (Pillow) not installed")
        print("Install with: pip install Pillow")
        sys.exit(1)
    
    # Batch mode or single file
    if args.batch:
        if not os.path.isdir(args.input):
            print("ERROR: Input must be a directory in batch mode")
            sys.exit(1)
        
        batch_convert(
            args.input,
            args.output,
            threshold=args.threshold,
            contrast=args.contrast,
            brightness=args.brightness,
            invert=args.invert,
            dither=args.dither
        )
    else:
        if not os.path.isfile(args.input):
            print(f"ERROR: Input file not found: {args.input}")
            sys.exit(1)
        
        success = convert_to_bin(
            args.input,
            args.output,
            threshold=args.threshold,
            contrast=args.contrast,
            brightness=args.brightness,
            invert=args.invert,
            dither=args.dither
        )
        
        sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
