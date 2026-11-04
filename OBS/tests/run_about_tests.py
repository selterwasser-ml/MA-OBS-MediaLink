"""Check the actual secret sequence and the exact embedded, unmodified photo."""
from pathlib import Path
import hashlib,os,struct,subprocess,tempfile,xml.etree.ElementTree as ET
root=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as temp:
    binary=str(Path(temp)/'about_test')
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(root/'about_secret_test.cpp'),'-o',binary],check=True)
    subprocess.run([binary],check=True)
resource=root.parent/'data/medialink.qrc'
item=ET.parse(resource).find('./qresource/file')
assert item is not None and item.attrib['alias']=='cats_farin_bela.png'
image=resource.parent/item.text
data=image.read_bytes()
assert data[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',data[16:24])==(1024,1365)
expected='1369ab0048ef779f185a2b03da54bda1b1cb4350e55401f45a68a11643b043b7'
assert hashlib.sha256(data).hexdigest()==expected
assert 'CMAKE_AUTORCC ON' in (root.parent/'CMakeLists.txt').read_text()
assert 'data/medialink.qrc' in (root.parent/'CMakeLists.txt').read_text()
print('PASS: Farin then Bela only, wrong order/reset/reopen, unchanged complete photo, Qt resource/build wiring')
