# Commands for working with the memory buffer via the 2nd channel
The JSON root must contain only one element: `"buf"`. The JSON root may contain other elements.
Only one command is transmitted at a time, the next command is only sent after receiving a response.
If an error occurs during command processing, the following response is issued:
```
{
    "buf":
    {
        "error":"error description"
    }
}
```
### 1.Create Buffer.
```
{
    "buf":
    {
        "create":1024,  // buffer size in bytes
        "part":200      // maximum packet size in bytes (optional)
    }
}
```
Response
```
{
    "buf":
    {
        {"ok":"Buf was created 1024(200)"}
    }
}
```
If successful, packets can be sent to the device via the 2nd channel.
### 2.Create Buffer from File.
```
{
    "buf":
    {
        "rd":"udp.json",    // filename
        "part":200      // maximum packet size in bytes (optional)
    }
}
```
Response
```
{
    "buf":
    {
        "fr":"udp.json",    // filename
        "ok":"buffer was loaded from udp.json",
        "size":170,   // file size in bytes
        "part":200    // maximum packet size in bytes
    }
}
```
If successful, the device starts transmitting packets via the 2nd channel.
### 3.Check Buffer Fill Level.
```
{
    "buf":
    {
        "check":null
    }
}
```
Response
```
{
    "buf":
    {
        "empty":[0]  // list of packet numbers that need to be sent to the device
    }
}
```
### 4.Write Buffer to File.
```
{
    "buf":
    {
        "wr":"t.dat", // filename
        "free":null // free the buffer after writing (optional)
    }
}
```
Response
```
{
    "buf":
    {
        "ok":"file t.dat was saved"
    }
}
```
### 5.Write Buffer as Firmware Update.
```
{
    "buf":
    {
        "ota":null, 
        "free":null // free the buffer after writing (optional)
    }
}
```
Response
```
{
    "buf":
    {
        "ok":"firmware was saved"
    }
}
```
### 6. Clear Buffer.
```
{
    "buf":
    {
        "free":null
    }
}
```
Response
```
{
    "buf":
    {
        "ok":"buffer was deleted"
    }
}
```
## 2nd Channel Data Format.
The first two bytes are the packet number, followed by the data (maximum packet size if not the last packet).