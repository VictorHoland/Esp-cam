# Softex Visão SDK - Manual do usuário
## Dependências
 * Python 3.9
 * Tensorflow 2.0
 * esp-idf v4.4.1
## Instalação
* Crie um projeto com o ESP-IDF
* Ative o gerenciador de componentes com 
```
IDF_COMPONENT_MANAGER=1
```
* Clone o repositório para a pasta de componentes
```
mkdir components
git clone https://github.com/edgebr/edge-visao-devkit-fw components
```

* Crie um ambiente virtual do Python
```
python -m venv <nome>
<nome>\Scripts\activate
pip install -r components/edge_softex/scripts/requirements.txt
```

## Conversão de modelo
* Use o script `tflite2c.py` para converter o modelo do arquivo `<modelo>.tflite` para os arquivos `modelo.h` e `modelo.cpp`
```
python components/edge_softex/scripts/tflite2c.py <caminho-para-modelo.tflite> <caminho-para-pasta-de-destino>
```
* Adicione o arquivo `<modelo>.cpp` à lista de fontes no arquivo `CMakeLists.txt` do componente main (`<projeto>/main/CMakeLists.txt`) para que ele seja incluído na compilação
```
idf_component_register(SRCS ... "<modelo>.cpp"  
                    INCLUDE_DIRS ".")
```
## Módulos
### camera
Configura a câmera e faz a captura das imagens. Utiliza o driver esp32-camera

#### Utilização
1. Configure a câmera através do comando `menuconfig` do ESP-IDF em 
```Component config > Edge Softex Vision Devkit > Input > Camera config```
2. No código da aplicação, configure o módulo com camera_setup(), passando o handle para a fila que receberá as imagens
```
int err = camera_setup(handle);
```
3. Obtenha as imagens:
	* De maneira automática com `camera_start()`
		```
		int err = camera_start(); 
		```
	* De maneira manual
		```
		camera_fb_t* frame = esp_camera_fb_get();
		xQueueSend(handle);
		esp_camera_fb_return(frame);
		```
### io
Configura e gerencia a saída do modelo, utilizando os pinos da placa ou através da rede, via protocolo MQTT

#### Utilização
1. Crie uma estrutura do tipo `io_config_t` com os parâmetros desejados. 
	```
	io_config_t io_config = {
		...
	};
	```
2. Inicialize o módulo com `io_setup()`
	```
	int err = io_setup(<io type>, &io_config, handle); 
	```
3. Inicie a tarefa de processamento da inferência
	```
	int err = io_start();
	``` 
#### Observações
* A estrutura deve permanecer válida durante toda a execução (variável global ou estática)
* Para uma saída do tipo MQTT é necessário que o dispositivo esteja conectado à rede

### streaming

#### Utilização
1. Inicie o streaming com `stream_start`
	```
	int err = stream_start(<tipo>, <fila_imagem_entrada>, <fila_imagem_saida>);
	```
2. Para pausar o streaming use a função `stream_stop()`

#### Observações
* Para que os protocolos fiquem disponíveis, é necessário habilitá-los através do `menuconfig`
* Para utilizar o protoculo customizado (`STREAM_PROTOCOL_CUSTOM`)

### tflite
Processamento e inferência do modelo

#### Utilização
1. Configure através do `menuconfig` em 
```Component config > Edge Softex Vision Devkit > Tensorflow Lite Micro Settings```
2. Crie uma variável do tipo `tflite_config_t` com os parâmetros desejados
	```
	tflite_config_t tflite_config = {
		...
		.model = (void*) <modelo>_array,
		...
	};
	```
3. Registre as operações utilizadas pelo modelo
	```
	<modelo>_register_operations();
	tflite_config.op_resolver = <modelo>_get_resolver();
	```
5. Na inicialização, configure o módulo com camera_setup(), passando o handle das filas que receberão a imagem, a saída do
	```
	int err = tflite_setup(&tflite_config, camera_input_handle, model_output_handle, camera_output_handle);
	```
6. Inicie a tarefa de inferência
		```
		int err = tflite_start(); 
		```
#### Observações
* O campo `tensor_arena_size` da estrutura `tflite_config_t`define o tamanho da região da memória que será utilizada para fazer o processamento do modelo. Esse valor varia de acordo com o modelo utilizado e deve ser escolhido pelo usuário
* É possível definir um *array* de *strings* com nomes para as classes através do campo `labels`, onde o índice da classe será 
		
### wifi

Módulo de auxílio para conexão com WiFi.

#### Utilização
1. Configure a câmera através do comando `menuconfig` do ESP-IDF em 
```Component config > Edge Softex Vision Devkit > Input > Camera config```
2. No código da aplicação, configure o módulo com camera_setup(), passando o handle para a fila que receberá as imagens
	```
	int err = wifi_setup(<nome_da_rede>, <senha>);
	```

#### Observações
* É possível configurar o nome da rede e a senha através do `menuconfig`. Nesse caso, os valores ficam disponíveis no código através das macros `SOFTEX_WIFI_SSID` e `SOFTEX_WIFI_PASSWORD`. 
