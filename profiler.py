
masks = 'SO,Poll,Pck,Tj,Im,NCS,Mov'


bits = {}

bits['SO'] = 'Creation,Bind,Listening,Accept,Send,Recv,Addr'

bits['Poll'] = 'Generic,Timeout,RecvBusy'

bits['Pck'] = 'Error,STX,TooSmall'

bits['Tj'] = 'Generic,Header,Decompress,Compress,Destroy'

bits['Im'] = 'WrongSize'

bits['NCS'] = 'DevCreate,DevOpen,GraphCreate,GraphAllocate,Inference,GetOpt,FifoRead,Destroy'

bits['Mov'] = 'ReadGraphFile,TooFewBytes'


header = ""
source = ""
m = 1
for mask in masks.split(','):
    header += "extern error " + mask + "Error;\n"
    source += "error {:10} = 1 << {:2};\n".format(mask + 'Mask', m)
    m+=1

header += "\n\n"
source += "\n\n"

m = 1
for mask in masks.split(','):
    b=1
    for bit in bits[mask].split(','):
        header += "extern error " + mask + bit + "Error;\n"
        source += ("ERROR({:>5}, {:5}, {:>15}, {:5});\n".format(mask, m, bit, b))
        b=b+1
    header += "\n"
    source += "\n"
    m=m+1

# print(header)
# print(source)

enum = ""
m = 1
for mask in masks.split(','):
    b=1
    enum += "typedef enum {\n"
    for bit in bits[mask].split(','):
        enum += "\t" + mask + bit + ",\n"
        b=b+1
    enum = enum[0:-2] + "\n} " + mask + "Error;\n\n"
    m=m+1

print(enum)