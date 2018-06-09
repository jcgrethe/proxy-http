# Situarse en la carpeta con los archivos pesados, preferentemente el server
cd ~/itba/proto/http-latest/webapp
echo "sha256sum del archivo 3G: "
sha256sum f3g


echo "=====    Inicio    ====="
echo "---- 000 GET 3G----"
time curl -x localhost:8080 -s http://localhost:9090/f3g | sha256sum
echo "---- 001 ----"
echo "Ejecutar en 6 terminales distintas:  time curl -x localhost:8080  -s http://localhost:9090/f3g |sha256sum"
echo "---- 002 Respuestas 204----"
curl -x localhost:8080  -i http://localhost:9090/api/other/bu http://localhost:9090/api/other/bu
echo "---- 003 POST basico ----"
curl -x localhost:8080  --data-binary 'hola' -X POST http://localhost:9090/api/other -i
echo "---- 004 POST 3G ----"
time curl -x localhost:8080  -s --data-binary @f3g -X POST http://localhost:9090/api/other |sha256sum
echo "---- 005 HEAD ----"
curl -x localhost:8080  -I http://localhost:9090/api/other http://localhost:9090/api/other
echo "---- 006 GET Condicional ----"
curl -x localhost:8080  -I http://localhost:9090/leet.txt
curl -x localhost:8080  -i -H'If-Modified-Since: Sat, 10 Jun 2018 22:45:23 GMT' http://localhost:9090/leet.txt
echo "---- 007 Multiples requests ----"
curl -x localhost:8080  -s http://www.mdzol.com/robots.txt http://mirrors.ibiblio.org/robots.txt
echo "---- 008 No resuelve ----"
curl -x localhost:8080  -i http://bar/
echo "---- 009 Conexion no disponible ----"
curl -x localhost:8080  -i http://127.0.0.1/
echo "---- 010 El proxy como o. server ----"
# curl -x localhost:8080  -i http://localhost:8080/
echo "---- 011 O. Server deja de estar en linea response ----"
echo "Manual."
echo "---- 012 O. Server deja de estar en linea request  ----"
echo "Manual."
echo "---- 013 L33t Content Lenght ----"
curl -x localhost:8080  -s http://localhost:9090/leet.txt | wc -l
echo "---- 014 L33t Chunked ----"
curl -x localhost:8080  -s -i http://localhost:9090/api/chunked/leet | wc -l
echo "---- 015 L33t Chunked Gzip ----"
curl -x localhost:8080  --compressed -s -i http://localhost:9090/api/chunked/leet | wc -l
echo "---- 016 Delete (No requerido) ----"
curl -x localhost:8080  -i -X DELETE http://localhost:9090/api/other
echo "---- 017 PUT basico (No requerido) ----"
curl -x localhost:8080  --data-binary 'hola' -X PUT http://localhost:9090/api/other -i
echo "---- 018 PUT 3G (No requerido) ----"
curl -x localhost:8080  -s --data-binary @f3g -X PUT http://localhost:9090/api/other



echo "======    Extras    ======"
echo "---- POST carcteres chinos ----"
curl -x localhost:8080  --data-binary '卡拉科特' -X POST http://localhost:9090/api/other -i
echo "---- Metodo SEARCH (Aplica para demas metodos) ----"
curl -x localhost:8080  --data-binary 'a' -X SEARCH http://localhost:9090/api/other
echo "======    FIN    ======"
