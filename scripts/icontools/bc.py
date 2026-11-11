import struct, io
from PIL import Image
def dds(blocks, w, h, dxgi):
    # DX10 header DDS
    hdr = b'DDS ' + struct.pack('<7I',124,0x1|0x2|0x4|0x1000|0x80000,h,w,len(blocks),0,1) + b'\0'*44
    hdr += struct.pack('<2I4s5I',32,4,b'DX10',0,0,0,0,0) + struct.pack('<5I',0x1000,0,0,0,0)
    hdr += struct.pack('<5I',dxgi,3,0,1,0)
    return hdr+blocks
def decode(blocks,w,h,dxgi):
    return Image.open(io.BytesIO(dds(blocks,w,h,dxgi))).convert('RGBA')
