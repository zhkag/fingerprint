from building import *

cwd  = GetCurrentDir()


src = []
inc = [cwd]

src += ['fingerprint.c']
src += ['fingerprint_sample.c']


group = DefineGroup('fingerprint', src, depend = ['PKG_USING_FINGERPRINT'], CPPPATH = inc)
Return('group')
