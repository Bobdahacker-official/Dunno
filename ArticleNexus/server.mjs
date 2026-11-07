import './build.mjs';
const {handle}=await import('./app.mjs');
const {createServer}=await import('node:http');
const port=Number(process.env.PORT||3000);
createServer(async(req,res)=>{try{const host=req.headers.host||`localhost:${port}`;const response=handle(new Request(new URL(req.url,process.env.PUBLIC_ORIGIN||`http://${host}`),{method:req.method,headers:req.headers}));res.writeHead(response.status,Object.fromEntries(response.headers));res.end(Buffer.from(await response.arrayBuffer()));}catch{res.writeHead(500);res.end('Internal server error');}}).listen(port,'0.0.0.0',()=>console.log(`ArticleNexus listening on ${port}`));
