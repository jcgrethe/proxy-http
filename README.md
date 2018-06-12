# Trabajo Practico Especial - Protocolos de comunicación
## Materiales entregados

* Informe: `documentacion/Informe.pdf`.
* Presentación: `documentacion/Presentación.pdf`.
* Compilacion: Archivo: `Makefile` 
* Códigos fuente varios
* Estadisticas de repositorio

## Compilación

Para compilar correr en el directorio raíz:
 
```
make clean all
```

### Artefactos generados
Archivos generados post compilacion.

* httpd: server proxy.
* sctpclnt: cliente de configuración.

## Ejecución
### proxy
El proxy httpd se ejecuta respetando lo sugerido por el 
manual `httpd.8`.  
```
./httpd [parametros]
```
### sctpclnt
El cliente de configuración se ejecuta corriendo:
```
./sctpclnt [options]
```