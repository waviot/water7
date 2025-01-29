﻿#include "WVT_Water7.h"

WVT_W7_Callbacks_t externals_functions;

WVT_W7_Error_t WVT_W7_Single_Parameter(
    uint16_t parameter_addres,
    WVT_W7_Parameter_Action_t action,
    uint8_t * responce_buffer);

/**
 * @brief	Формирует стартовый пакет, указывающий на начало работы устройства
 *			после включения питания или перезагрузки
 *
 * @param   resets		   	            Указатель на буфер с выходными данными
 * @param   [out] responce_buffer	  	Число перезагрузок устройства
 * 
 * @returns	Число зачисанных байт в буфер с выходными данными.
 */
uint8_t WVT_W7_Start(int32_t resets, uint8_t * responce_buffer)
{
    if (resets < 0)
    {
        resets = 0;
    }
    
    return WVT_W7_Event(WVT_W7_EVENT_RESET, (uint16_t) resets, responce_buffer);
}

/**
 * @brief	Регистрирует функции, используемые библиотекой для работы с физическими интерфейсами
 *
 * @param   callbacks		   	Структура с адресами функций
 * 
 * @return  - WVT_W7_OK Регистрация успешна
 *          - WVT_W7_ERROR Неверные указатели 
 */
WVT_W7_Status_t WVT_W7_Register_Callbacks(WVT_W7_Callbacks_t callbacks)
{
    if (    (callbacks.rom_read != 0)
        &&  (callbacks.rom_write != 0)  )
    {
        externals_functions = callbacks;
        return WVT_W7_OK;
    }

    return WVT_W7_ERROR;
}

/**
 * @brief	Обрабатывает входящий NB-Fi пакет
 *
 * @param [in] 	data		   	Указатель на буфер с входными данными
 * @param 	   	length		   	Чило байт во входном буфере
 * @param [out]	responce_buffer	Указатель на буфер с выходными данными
 *
 * @returns	Число зачисанных байт в буфер с выходными данными.
 */
uint8_t WVT_W7_Parse(uint8_t * data, uint16_t length, uint8_t * responce_buffer)
{
    WVT_W7_Error_t return_code = WVT_W7_ERROR_CODE_OK;
    uint16_t responce_length;
    uint32_t addres;
    uint32_t number_of_parameters;
    
    // Должны быть переданы верные указатели на данные
    if ((data && length && responce_buffer) == 0)
    {
        return 0;	
    }
    
    WVT_W7_Packet_t packet_type = (WVT_W7_Packet_t) data[0];
    
    switch (packet_type)
    {
    case WVT_W7_PACKET_TYPE_READ_MULTIPLE:
        addres = (data[1] << 8) + data[2];
        number_of_parameters = (data[3] << 8) + data[4];
        
        if (     (length == WVT_W7_READ_MULTIPLE_LENGTH)
            &&  ((WVT_W7_MULTI_DATA_OFFSET + (number_of_parameters * WVT_W7_PARAMETER_WIDTH)) <= WVT_W7_BUFFER_SIZE) )
        {
            // Тип сообщения, адрес начала последовательности и длинна последовательности
            // заполняются из входящего пакета
            for (uint8_t i = 0 ; i < WVT_W7_MULTI_DATA_OFFSET ; i++)
            {
                responce_buffer[i] = data[i];
            }
            
            uint16_t current_parameter = 0;
            while (	(return_code == WVT_W7_ERROR_CODE_OK)
                &&	(current_parameter < number_of_parameters)	)
            {
                return_code = WVT_W7_Single_Parameter((addres + current_parameter), WVT_W7_PARAMETER_READ, 
                    (responce_buffer + WVT_W7_MULTI_DATA_OFFSET + (current_parameter * WVT_W7_PARAMETER_WIDTH)));
                current_parameter++;
            }
            responce_length = WVT_W7_READ_MULTIPLE_LENGTH + (number_of_parameters * WVT_W7_PARAMETER_WIDTH);
        }
        else
        {
            return_code = WVT_W7_ERROR_CODE_INVALID_LENGTH;
        }
        break;
    case WVT_W7_PACKET_TYPE_WRITE_MULTIPLE:
        addres = (data[1] << 8) + data[2];
        number_of_parameters = (data[3] << 8) + data[4];
        
        if (length == ((number_of_parameters * WVT_W7_PARAMETER_WIDTH) + WVT_W7_MULTI_DATA_OFFSET))
        {
            // Тип сообщения, адрес начала последовательности и длинна последовательности
            // заполняются из входящего пакета
            for (uint8_t i = 0 ; i < WVT_W7_MULTI_DATA_OFFSET ; i++)
            {
                responce_buffer[i] = data[i];
            }
            
            uint16_t current_parameter = 0;
            while (     (return_code == WVT_W7_ERROR_CODE_OK)
                    &&	(current_parameter < number_of_parameters) )
            {
                return_code = WVT_W7_Single_Parameter((addres + current_parameter),
                    WVT_W7_PARAMETER_WRITE, 
                    (data + WVT_W7_MULTI_DATA_OFFSET + (current_parameter * WVT_W7_PARAMETER_WIDTH)));
                current_parameter++;
            }
            // Не опечатка
            responce_length = WVT_W7_READ_MULTIPLE_LENGTH;
        }
        else
        {
            return_code = WVT_W7_ERROR_CODE_INVALID_LENGTH;
        }
        break;
    case WVT_W7_PACKET_TYPE_READ_SINGLE:
        addres = (data[1] << 8) + data[2];
        
        if (length == WVT_W7_READ_SINGLE_LENGTH) 
        {
            // Тип сообщения и адрес заполняются из входящего пакета
            for(uint8_t i = 0 ; i < WVT_W7_SINGLE_DATA_OFFSET ; i++)
            {
                responce_buffer[i] = data[i];
            }
            
            return_code = WVT_W7_Single_Parameter(addres, 
                WVT_W7_PARAMETER_READ,
                (responce_buffer + WVT_W7_SINGLE_DATA_OFFSET));
            responce_length = WVT_W7_READ_SINGLE_LENGTH + WVT_W7_PARAMETER_WIDTH;
        }
        else
        {
            return_code = WVT_W7_ERROR_CODE_INVALID_LENGTH;
        }
        break;
    case WVT_W7_PACKET_TYPE_WRITE_SINGLE:
        addres = (data[1] << 8) + data[2];
        
        if (length == WVT_W7_WRITE_SINGLE_LENGTH) 
        {
            // Тип сообщения и адрес заполняются из входящего пакета
            for(uint8_t i = 0 ; i < WVT_W7_WRITE_SINGLE_LENGTH ; i++)
            {
                responce_buffer[i] = data[i];
            }
            
            return_code = WVT_W7_Single_Parameter(addres, 
                WVT_W7_PARAMETER_WRITE,
                (data + WVT_W7_SINGLE_DATA_OFFSET));
            responce_length = WVT_W7_WRITE_SINGLE_LENGTH;
        }
        else
        {
            return_code = WVT_W7_ERROR_CODE_INVALID_LENGTH;
        }
        break;
    case WVT_W7_PACKET_TYPE_FW_UPDATE:
        if (    (externals_functions.rfl_handler == 0)
            ||  (externals_functions.rfl_command == 0)   )
        {
            return_code = WVT_W7_ERROR_CODE_INVALID_TYPE;
            break;
        }

        if (length < 2)
        {
            return_code = WVT_W7_ERROR_CODE_INVALID_LENGTH;
            break;
        }
        
        return_code = externals_functions.rfl_handler(data, length, responce_buffer, &responce_length);
       
        break;
    case WVT_W7_PACKET_TYPE_CONTROL:
        if (    (externals_functions.rfl_handler == 0)
            ||  (externals_functions.rfl_command == 0)   )
        {
            return_code = WVT_W7_ERROR_CODE_INVALID_TYPE;
            break;
        }

        if (length != 7)
        {
            return_code = WVT_W7_ERROR_CODE_INVALID_LENGTH;
            break;
        }
        
        return_code = externals_functions.rfl_command(data, length, responce_buffer, &responce_length);
        
        break;
    default:
        return_code = WVT_W7_ERROR_CODE_INVALID_TYPE;
        break;
    }
    
    if (return_code == WVT_W7_ERROR_CODE_OK)
    {
        responce_buffer[0] = packet_type;
        return responce_length;
    }
    else
    {
        responce_buffer[0] = (packet_type | WVT_W7_ERROR_FLAG);
        responce_buffer[1] = return_code;
        return WVT_W7_ERROR_RESPONCE_LENGTH;
    }
}

/**
 * @brief	Читает или записывает один параметр из EEPROM в буфер.
 *			Для чтения и записи используется один и тот же указатель на буфер
 *			Данные размещаются начиная с нулевого смещения и занимают четыре байта
 *			Порядок байт: от старшего к младшему
 *
 * @param 	   		parameter_addres	   	Адрес параметра
 * @param 	   		action	   				Действие: чтение или запись
 * @param [in/out]	responce_buffer			Из этого буфера будут прочитанны или записаны данные
 *
 * @returns	- WVT_W7_ERROR_CODE_OK		        Параметр успешно записан в EEPROM
 *          - WVT_W7_ERROR_CODE_INVALID_ADDRESS Передан нулевой указатель
 * 			- WVT_W7_ERROR_CODE_LL_ERROR	    Произошла ошибка при записи
 */
WVT_W7_Error_t WVT_W7_Single_Parameter(
    uint16_t parameter_addres,
    WVT_W7_Parameter_Action_t action,
    uint8_t * responce_buffer)
{
    if (responce_buffer == 0)
    {
        return WVT_W7_ERROR_CODE_INVALID_ADDRESS;
    }
    
    WVT_W7_Error_t rom_operation_result = WVT_W7_ERROR_CODE_OK;
    int32_t value;
    
    if (action == WVT_W7_PARAMETER_READ)
    {
        rom_operation_result = externals_functions.rom_read(parameter_addres, &value) ;
        if (rom_operation_result == WVT_W7_ERROR_CODE_OK)
        {
            responce_buffer[0] = (value >> 24);
            responce_buffer[1] = (value >> 16);
            responce_buffer[2] = (value >> 8);
            responce_buffer[3] =  value;		
        }
    }
    else
    {
        value =   (responce_buffer[0] << 24) 
                + (responce_buffer[1] << 16)
                + (responce_buffer[2] << 8) 
                +  responce_buffer[3];
        rom_operation_result = externals_functions.rom_write(parameter_addres, value);
    }
    
    return rom_operation_result;
}

/**
 * @brief	    Формирует пакет о событии
 *
 * @param 	   	event		   	Тип события.
 * @param 	   	payload		   	Полезная нагрузка события
 * @param [out]	responce_buffer	Выходной буфер с сообщением NB-Fi.
 *
 * @returns	Число байт, записанных в выходной буфер.
 */
uint8_t WVT_W7_Event(uint16_t event, uint16_t payload, uint8_t * responce_buffer)
{
    responce_buffer[0] = WVT_W7_PACKET_TYPE_EVENT;
    responce_buffer[1] = (event >> 8);
    responce_buffer[2] =  event;
    responce_buffer[3] = (payload >> 8);
    responce_buffer[4] =  payload;
    
    return 5;
}

/**
 * @brief	    Формирует часовой парный пакет
 *
 * @param 	   	par		   	    Передаваемый параметр
 * @param 	   	value		   	Передаваемое значение параметра
 * @param 	   	diff		   	Разниза с value час назад
 * @param [out]	responce_buffer	Выходной буфер с сообщением NB-Fi.
 *
 * @returns	Число байт, записанных в выходной буфер.
 */
uint8_t WVT_W7_PairEvent(uint8_t par, uint32_t value, uint16_t diff,  uint8_t * responce_buffer)
{
    responce_buffer[0] = WVT_W7_PACKET_TYPE_PAIR_EVENT;
    responce_buffer[1] = par;
    responce_buffer[2] = (value >> 24);
    responce_buffer[3] = (value >> 16);
    responce_buffer[4] = (value >> 8);
    responce_buffer[5] = (value >> 0);
    responce_buffer[6] = (diff >> 8);
    responce_buffer[7] =  diff & 0xFF;   
    return 8;
}

/**
 * @brief	    Распаковывает значение настройки дополнительных параметров
 *              Возвращает число параметров, настроенных для отправки и записывает
 *              их адреса во входной массив
 *
 * @param [out]	parameters	    Массив, куда будут помещены адреса обнаруженных параметров (5 байт)
 * @param 	   	setting		   	Упакованное значение настройки (из EEPROM)
 *
 * @returns	    Число обнаруженных параметров
 */
uint8_t WVT_W7_Parse_Additional_Parameters(uint8_t * parameters, int32_t setting)
{
    uint8_t parameter_number = 0;
    const uint8_t parameter_mask = 63;

    while (setting & parameter_mask)
    {
        parameters[parameter_number] = setting & parameter_mask; 
        parameter_number++;
        setting = (setting >> 6);
    }
    
    return parameter_number;
}

/**
 * @brief		Формирует массив, готовый, для "приклеивания" к регулярному сообщению
 *
 * @param 	   	address		Адрес параметра
 * @param [out]	data		Указатель на буфер с выходными данными
 */
static void WVT_W7_Additional_Parameter(uint16_t address, uint8_t * data)
{
    int32_t parameter_value;
    externals_functions.rom_read(address, &parameter_value) ;
    
    data[0] = address;
    data[1] = (parameter_value >> 24);
    data[2] = (parameter_value >> 16);
    data[3] = (parameter_value >> 8);
    data[4] = (parameter_value);
}

/**
 * @brief		Формирует короткое регулярное сообщение
 *              Если настроены дополнительные параметры, то они будут добавлены 
 *              к регулярному сообщению
 *
 * @param [out]	responce_buffer		    Указатель на буфер с выходными данными
 * @param 	   	schedule		        Периодичность отправки регулярного сообщения (упакованный формат)
 * @param 	   	payload		            Адрес параметра
 * @param       additional_parameters   Упакованное значение настройки (из EEPROM)
 *                                      Значение 0 отключит отправку дополнительных параметров
 * 
 * @returns	    Число записанных байт
 */
uint8_t WVT_W7_Short_Regular(
    uint8_t * responce_buffer,
    int32_t payload,
    uint8_t parameter_number,
    uint16_t schedule, 
    int32_t additional_parameters)
{
 
    if (parameter_number > 63)
    {
        return 0;
    }

    uint8_t parameters[5];
    const uint8_t number_of_additional_params = WVT_W7_Parse_Additional_Parameters(parameters, 
        additional_parameters);

    responce_buffer[0] = (WVT_W7_REGULAR_MESSAGE_FLAG | parameter_number);
    responce_buffer[1] = (schedule >> 8);
    responce_buffer[2] = schedule;
    
    responce_buffer[3] = (payload >> 24);
    responce_buffer[4] = (payload >> 16);
    responce_buffer[5] = (payload >> 8);
    responce_buffer[6] = (payload);
    
    for (int i = 0; i < number_of_additional_params; i++)
    {
        WVT_W7_Additional_Parameter(parameters[i], 
		    ((responce_buffer + 7) + (i * WVT_W7_ADDITIONAL_DATA_WIDTH)));
    }
    
	return (WVT_W7_ADDITIONAL_DATA_OFFSET + (WVT_W7_ADDITIONAL_DATA_WIDTH * number_of_additional_params));
}	

/**
 * @brief       Рассчитывает, нужно ли сейчас отправлять периодическое сообщение, исходя
 *              из желаемоего количества отправок в день. Распределяет события равномерно
 *              в течении дня.
 *              Если для текущей пары час-минута срабатывает событие, то следующий вызов функции
 *              с этой же парой не приведет к срабатыванию события. 
 * 
 * @param current_hour    Текущий час
 * @param current_minute  Текущая минута
 * @param schedule        Желаемое число отправок в день
 * @return                - 0 нет события
 *                        - 1 событие 
 */
uint8_t WVT_W7_Scheduler(uint8_t current_hour, uint8_t current_minute, int32_t schedule)
{	
	static uint8_t triggered = 0;
	uint8_t ret = 0;

	// Частота отправки равна числу минут в день, деленных на необходимое число сообщений
	schedule = (24 * 60) / schedule;
	
	const uint32_t minutes_since_beginning = (current_hour * 60) + current_minute;
	
	const uint8_t time_has_come = (minutes_since_beginning % schedule) == 0;
	
	if (time_has_come)
	{
		if (triggered == 0)
		{
			ret = 1;
			triggered++;
		}
	}
	else
	{
		triggered = 0;
	}
	
	return ret;
}


/**
 * @brief       Рассчитывает, нужно ли сейчас отправлять периодическое сообщение, исходя
 *              из желаемоего количества отправок в день. Распределяет события равномерно
 *              в течении дня.
 *              Если для текущей комбинации час-минута-секунда срабатывает событие, то следующий вызов функции
 *              с этой же комбинацией не приведет к срабатыванию события. 
 * 
 * @param current_hour    Текущий час
 * @param current_minute  Текущая минута
 * @param current_second  Текущая секунда
 * @param schedule        Желаемое число отправок в день
 * @return                - 0 нет события
 *                        - 1 событие 
 */
uint8_t WVT_W7_PrecisionScheduler(uint8_t current_hour, uint8_t current_minute, uint8_t current_second, int32_t schedule)
{	
    uint8_t ret = 0;
    static uint32_t next_execution_time = 0;
    static uint8_t wait_new_day = 0;
    static uint8_t last_hour = 0;
    // Частота отправки равна числу секунд в день, деленных на необходимое число сообщений
    uint32_t newschedule = (24 * 3600) / schedule;
    uint32_t seconds_since_beginning = (uint32_t)((current_hour * 3600) + current_minute * 60 + current_second);
    if (current_hour < last_hour) wait_new_day = 0;
    if (seconds_since_beginning >= next_execution_time && wait_new_day == 0)
    {
        ret = 1;
        next_execution_time = (uint32_t)(seconds_since_beginning + newschedule);
        if (next_execution_time >= 24 * 60 * 60)
        {
            wait_new_day = 1;
            next_execution_time -= 24 * 60 * 60;
        }
    }
    last_hour = current_hour;
    return ret;
}
