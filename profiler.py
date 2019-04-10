
masks = 'SO,Poll,Pck,Tj,Im'


bits = {}

bits['SO'] = 'Creation,Bind,Listening,Accept,Send,Recv,Addr'

bits['Poll'] = 'Generic,Timeout,RecvBusy'

bits['Pck'] = 'Error,STX,TooSmall'

bits['Tj'] = 'Generic,Header,Decompress,Compress,Destroy'

bits['Im'] = 'WrongSize'

m = 1
for mask in masks.split(','):
    b=1
    for bit in bits[mask].split(','):
        print("ERROR({:>5}, {:5}, {:>12}, {:5});".format(mask, m, bit, b))
        b=b+1
    print("")
    m=m+1