//
// Created by vnrju on 08/30/2022.
//

#ifndef SOFTEX_TFLITE_H
#define SOFTEX_TFLITE_H

#include <tensorflow/lite/micro/micro_op_resolver.h>
#include "edge_softex.h"


/**
 * @brief Tipo de problema
 */
typedef enum {
    TASK_CLASSIFICATION,                               //!< Problema de classificação
    TASK_REGRESSION,                                   //!< Problema de regressão
} task_type_t;

/**
 * @brief Estrutura para armazenar o resultado de um problema de regressão.
 * */
typedef struct {
    size_t len;                                        //!< Quantidade de neurônios na saída da rede
    float *data;                                       //!< Array com a saída de cada neurônio
} regression_result_t;

/**
 * @brief Estrutura para armazenar o resultado de um problema de classificação
 * */
typedef struct {
    int argmax;                                        //!< Índice do neurônio com maior valor na saída. Representa a classe à qual a entrada dada para o modelo pertence
    float probability;                                 //!< Valor da saída do neurônio de índice @ref argmax. Representa a probabilidade da entrada dada para o modelo pertencer à classe @ref argmax
    char *label;                                       //!< Nome da classe (Opcional)
} classification_result_t;

/**
 * @brief Estrutura para armazenar a saída de um modelo de classificação ou (exclusivo) de regressão.
 * */
typedef struct {
    task_type_t task_type;                             //!< Tipo do problema. Deve ser checado antes de acessar os campos @ref classification e @ref regression

    union {
        classification_result_t classification;        //!< Dados de classificação
        regression_result_t regression;                //!< Dados para os problemas de regressão
    };
} model_output_t;

/**
 * @brief Estrutura para armazenar os parâmetros de configuração do modelo
 * */
typedef struct {
    task_type_t task;                                  //!< Tipo de problema (@ref TASK_CLASSIFICATION ou @ref TASK_REGRESSION)
    size_t tensor_arena_size;                          //!< Tamanho da região de memória apontada por @ref tensor_arena
    size_t output_len;                                 //!< Quantidade de neurônios na camada de saída da rede
    void* model;                                       //!< Ponteiro para o modelo
    tflite::MicroOpResolver *op_resolver;              //!< Ponteiro para a estrutura com as operações utilizadas pelo modelo
    uint8_t *tensor_arena;                             //!< Ponteiro para a memória utilizada para os cálculos durante o processamento
    const char **labels;                               //!< Nomes das classes (opcional)

} tflite_config_t;


/**
 * @brief Configura o modelo do Tensorflow Lite.
 *
 * @param config Parâmetros de configuração do modelo
 *
 * @return
 *      - 0 caso o modelo seja configurado com sucesso
 *      - -ENOSUP caso a versão do modelo seja incompatível com o Tensorflow Lite Micro
 *      - -ENOSR caso não seja possível alocar a memória necessária para o modelo
 *      - -ERANGE caso o tamanho da entrada configurado seja diferente do tamanho esperado pelo modelo
 *      - -ENOSR caso o tipo do problema seja @a TASK_REGRESSION e não seja possível alocar a memória para armazenar a saída do modelo
 */
int tflite_setup(tflite_config_t *config, QueueHandle_t image_input_queue, QueueHandle_t model_output_queue,
                 QueueHandle_t image_output_queue);

/**
 * @brief Inicia a tarefa de inferência.
 *
 * @return
 *      - 0 caso a tarefa inicie com sucesso
 *      - EFAULT caso não seja possível criar a tarefa
 */
int tflite_start();

#endif //SOFTEX_TFLITE_H