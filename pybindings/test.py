from tms9918 import Tms9918
from PIL import Image

t=Tms9918()

d=open("image.bin","rb").read()
vram=d[:16*1024]
regs=d[16*1024:]


t.setRegs(list(regs))
t.setVram(0,list(vram))


img = Image.frombytes('RGB', (256, 192), bytes(t.getScreen()))
img.show()
