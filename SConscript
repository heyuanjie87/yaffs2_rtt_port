# RT-Thread building script for component

from building import *

cwd = GetCurrentDir()

src = Glob('*.c')
comm = Glob('yaffs/*.c')
direct = Glob('yaffs/direct/*.c')

src = src + comm + direct

depend = ['RT_USING_DFS', 'RT_USING_MTD', 'PKG_USING_DFS_YAFFS']
CPPPATH = [cwd + '/yaffs', cwd + '/yaffs/direct']

group = DefineGroup('yaffs', src , depend = depend, CPPPATH = CPPPATH)

Return('group')
