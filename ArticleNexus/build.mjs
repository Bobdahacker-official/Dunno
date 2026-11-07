import {readFile,writeFile,mkdir,copyFile} from 'node:fs/promises';
for(const [source,target] of [['style.css','style-text.mjs'],['client.js','client-text.mjs']])await writeFile(target,'export default '+JSON.stringify(await readFile(source,'utf8'))+';\n');
await mkdir('dist/server',{recursive:true});
for(const file of ['app.mjs','articles.mjs','style-text.mjs','client-text.mjs'])await copyFile(file,'dist/server/'+file);
await writeFile('dist/server/index.js',"export {default} from './app.mjs';\n");
console.log('Worker built in dist/server/index.js');
