from PIL import Image
import os

def resize_png(input_path, output_path, scale_factor=0.25):
    try:
        with Image.open(input_path) as img:
            # 确保保持 RGBA 模式以维持透明度
            if img.mode != 'RGBA':
                img = img.convert('RGBA')
            
            new_size = (int(img.width * scale_factor), int(img.height * scale_factor))
            print(f"Resizing {input_path} from {img.size} to {new_size}...")
            
            # 使用 Resampling.LANCZOS 获得高质量缩放
            resized_img = img.resize(new_size, Image.Resampling.LANCZOS)
            resized_img.save(output_path, "PNG")
            print(f"Saved to {output_path}")
    except Exception as e:
        print(f"Error processing {input_path}: {e}")

if __name__ == "__main__":
    files_to_process = [
        ("./Picture/190overall7.png", "./Picture/190overall7_small.png"),
        ("./Picture/辅助装配0327.3.png", "./Picture/辅助装配0327.3_small.png")
    ]
    
    for input_p, output_p in files_to_process:
        if os.path.exists(input_p):
            resize_png(input_p, output_p)
        else:
            print(f"File not found: {input_p}")
