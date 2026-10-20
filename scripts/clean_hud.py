import re

p1 = r'c:\Users\ADMIN\Documents\GitHub\FnFNitroInFH2\dist\mediaoverride\ui\Controls\HUDSpeedometerControl.xaml'
p2 = r'D:\Games\FH2\Anthem_1.0.0.64_x64__8wekyb3d8bbwe\Mount\mediaoverride\ui\Controls\HUDSpeedometerControl.xaml'

with open(p1, 'r', encoding='utf-8') as f:
    text = f.read()

# 1. Remove A_FZGeometry_RadialNosFill from resources
text = re.sub(r'\s*<local:FZGeometry x:Key="A_FZGeometry_RadialNosFill"[^>]*/>', '', text)

# 2. Remove the broken geometry fill material element
text = re.sub(r'\s*<!-- Authentic Fast & Furious NOS Radial Arc Fill.*?/>', '', text, flags=re.DOTALL)

with open(p1, 'w', encoding='utf-8') as f:
    f.write(text)

with open(p2, 'w', encoding='utf-8') as f:
    f.write(text)

print('Cleaned up HUDSpeedometerControl.xaml successfully.')
