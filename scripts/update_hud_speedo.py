import os
import shutil
import xml.etree.ElementTree as ET

def update_hud():
    p1 = r'c:\Users\ADMIN\Documents\GitHub\FnFNitroInFH2\dist\mediaoverride\ui\Controls\HUDSpeedometerControl.xaml'
    p2 = r'D:\Games\FH2\Anthem_1.0.0.64_x64__8wekyb3d8bbwe\Mount\mediaoverride\ui\Controls\HUDSpeedometerControl.xaml'

    # Validate XML integrity first
    with open(p1, 'r', encoding='utf-8') as f:
        content = f.read()

    try:
        ET.fromstring(content)
        print("XAML validation passed: XML is completely well-formed!")
    except Exception as e:
        print(f"Error: XAML validation failed: {e}")
        return

    # Deploy to game mount
    if os.path.exists(os.path.dirname(p2)):
        shutil.copyfile(p1, p2)
        print(f"Successfully deployed valid HUDSpeedometerControl.xaml to game mount:\n  {p2}")
    else:
        print(f"Warning: Mount path not found: {p2}")

if __name__ == '__main__':
    update_hud()
