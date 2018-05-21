const net = require('net');
const server = net.createServer((socket) => {
  console.log('\n\n\---client connected---');
  socket.on('data',(data)=>{
      console.log("Msg from client :"+data.toString());
  });
  socket.end('Hello TCP Client\n'); // writes the given data and sends FIN packet
}).on('error', (err) => {
  console.log("err:"+err);
  throw err;
});

var os = require('os');

var interfaces = os.networkInterfaces();
var addresses = [];
for (var k in interfaces) {
    for (var k2 in interfaces[k]) {
        var address = interfaces[k][k2];
        if (address.family === 'IPv4' && !address.internal) {
            addresses.push(address.address);
        }
    }
}

console.log(addresses);


server.listen({
  host: '0.0.0.0',
  port: 3010,
  exclusive: true
},() => {
  console.log('TCP server started');
});