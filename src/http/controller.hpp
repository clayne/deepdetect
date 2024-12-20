/**
 * DeepDetect
 * Copyright (c) 2020 Jolibrain SASU
 * Author: Mehdi Abaakouk <mehdi.abaakouk@jolibrain.com>
 *
 * This file is part of deepdetect.
 *
 * deepdetect is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * deepdetect is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with deepdetect.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CRUD_STATICCONTROLLER_HPP
#define CRUD_STATICCONTROLLER_HPP

#include <memory>
#include <vector>
#include <iostream>

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"

#include "apidata.h"
#include "oatppjsonapi.h"
#include "utils/utils.hpp"
#include "dto/info.hpp"
#include "dto/service_predict.hpp"
#include "dto/service_create.hpp"
#include "dto/stream.hpp"
#include "dto/resource.hpp"
#include "documentation.hpp"

#include OATPP_CODEGEN_BEGIN(ApiController)

class DedeController : public oatpp::web::server::api::ApiController
{
public:
  DedeController(dd::OatppJsonAPI *oja,
                 const std::shared_ptr<ObjectMapper> &objectMapper)
      : oatpp::web::server::api::ApiController(objectMapper), _oja(oja)
  {
  }

private:
  dd::OatppJsonAPI *_oja = nullptr;

public:
  static std::shared_ptr<DedeController>
  createShared(dd::OatppJsonAPI *oja = nullptr,
               OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper))
  {
    return std::make_shared<DedeController>(oja, objectMapper);
  }

  ENDPOINT_INFO(get_info)
  {
    info->summary = "Returns general information about the deepdetect server, "
                    "including the list of existing services.";
    info->addResponse<Object<dd::DTO::InfoResponse>>(Status::CODE_200,
                                                     "application/json");
  }
  ENDPOINT("GET", "info", get_info, QUERIES(QueryParams, queryParams))
  {
    auto info_resp = dd::DTO::InfoResponse::createShared();
    info_resp->head = dd::DTO::InfoHead::createShared();
    info_resp->head->services = {};

    oatpp::String qs_status = queryParams.get("status");
    bool status = false;
    try
      {
        if (qs_status)
          status = dd::dd_utils::parse_bool(*qs_status);
      }
    catch (boost::bad_lexical_cast &)
      {
        return _oja->response_bad_request_400(
            "status must be a boolean value");
      }

    auto hit = _oja->_mlservices.begin();
    while (hit != _oja->_mlservices.end())
      {
        auto service_info = mapbox::util::apply_visitor(
            dd::visitor_info(status), (*hit).second);
        info_resp->head->services->emplace_back(service_info);
        ++hit;
      }
    return createDtoResponse(Status::CODE_200, info_resp);
  }

  ENDPOINT_INFO(get_service)
  {
    info->summary = "Returns information on an existing service";
  }
  ENDPOINT("GET", "services/{service-name}", get_service,
           PATH(oatpp::String, service_name, "service-name"),
           QUERIES(QueryParams, queryParams))
  {
    oatpp::String qs_status = queryParams.get("status");
    bool status = true;
    try
      {
        if (qs_status)
          status = dd::dd_utils::parse_bool(*qs_status);
      }
    catch (boost::bad_lexical_cast &)
      {
        return _oja->response_bad_request_400(
            "status must be a boolean value");
      }

    oatpp::String qs_labels = queryParams.get("labels");
    bool labels = false;
    try
      {
        if (qs_labels)
          labels = dd::dd_utils::parse_bool(*qs_labels);
      }
    catch (boost::bad_lexical_cast &)
      {
        return _oja->response_bad_request_400(
            "labels must be a boolean value");
      }

    auto janswer = _oja->service_status(service_name, status, labels);
    return _oja->jdoc_to_response(janswer);
  }

  ENDPOINT_INFO(create_service)
  {
    info->summary = "Create a new machine learning service";
    info->addConsumes<Object<dd::DTO::ServiceCreate>>("application/json");
  }
  ENDPOINT("POST", "services/{service-name}", create_service,
           PATH(oatpp::String, service_name, "service-name"),
           BODY_STRING(oatpp::String, service_data))
  {
    auto janswer = _oja->service_create(service_name, service_data);
    return _oja->jdoc_to_response(janswer);
  }

  ENDPOINT_INFO(update_service)
  {
    // Don't document PUT, it's a dup of POST, maybe deprecate it later
    info->hide = true;
  }
  ENDPOINT("PUT", "services/{service-name}", update_service,
           PATH(oatpp::String, service_name, "service-name"),
           BODY_STRING(oatpp::String, service_data))
  {
    auto janswer = _oja->service_create(service_name, service_data);
    return _oja->jdoc_to_response(janswer);
  }
  ENDPOINT_INFO(delete_service)
  {
    info->summary = "Delete a service";
    info->queryParams.add("clear", String::Class::getType()).description
        = "`full`, `lib`, `mem`, `dir` or `index`. `full` clears the model "
          "and service repository, `lib` removes model files only according "
          "to the behavior specified by the service's ML library, `mem` "
          "removes the service from memory without affecting the files, `dir` "
          "removes the whole directory, `index` removes the index when using "
          "similarity search. default=`mem`";
  }
  ENDPOINT("DELETE", "services/{service-name}", delete_service,
           PATH(oatpp::String, service_name, "service-name"),
           QUERIES(QueryParams, queryParams))
  {
    std::string jsonstr = _oja->uri_query_to_json(queryParams);
    auto janswer = _oja->service_delete(service_name, jsonstr);
    return _oja->jdoc_to_response(janswer);
  }

  ENDPOINT_INFO(predict)
  {
    info->summary = "Predict";
    info->addConsumes<Object<dd::DTO::ServicePredict>>("application/json");
  }
  ENDPOINT("POST", "predict", predict,
           BODY_STRING(oatpp::String, predict_data))
  {
    auto janswer = _oja->service_predict(predict_data);
    return _oja->jdoc_to_response(janswer);
  }

  ENDPOINT_INFO(get_train)
  {
    info->summary = "Retrieve a training status";
  }
  ENDPOINT("GET", "train", get_train, QUERIES(QueryParams, queryParams))
  {
    std::string jsonstr = _oja->uri_query_to_json(queryParams);
    auto janswer = _oja->service_train_status(jsonstr);
    return _oja->jdoc_to_response(janswer);
  }

  ENDPOINT_INFO(post_train)
  {
    info->summary
        = "Launches a blocking or asynchronous training job from a service.";
    info->body.description = dd::GET_TRAIN_PARAMETERS();
  }
  ENDPOINT("POST", "train", post_train, BODY_STRING(oatpp::String, train_data))
  {
    auto janswer = _oja->service_train(train_data);
    return _oja->jdoc_to_response(janswer);
  }

  ENDPOINT_INFO(put_train)
  {
    // Don't document PUT, it's a dup of POST, maybe deprecate it later
    info->hide = true;
  }
  ENDPOINT("PUT", "train", put_train, BODY_STRING(oatpp::String, train_data))
  {
    auto janswer = _oja->service_train(train_data);
    return _oja->jdoc_to_response(janswer);
  }
  ENDPOINT_INFO(delete_train)
  {
    info->summary = "Stop and delete a training job";
  }
  ENDPOINT("DELETE", "train", delete_train, QUERIES(QueryParams, queryParams))
  {
    std::string jsonstr = _oja->uri_query_to_json(queryParams);
    auto janswer = _oja->service_train_delete(jsonstr);
    return _oja->jdoc_to_response(janswer);
  }

  ENDPOINT_INFO(create_chain)
  {
    info->summary = "Run a chain call, that allows to call multiple models "
                    "sequentially";
  }
  ENDPOINT("POST", "chain/{chain-name}", create_chain,
           PATH(oatpp::String, chain_name, "chain-name"),
           BODY_STRING(oatpp::String, chain_data))
  {
    auto janswer = _oja->service_chain(chain_name, chain_data);
    return _oja->jdoc_to_response(janswer);
  }

  ENDPOINT_INFO(update_chain)
  {
    // Don't document PUT, it's a dup of POST, maybe deprecate it later
    info->hide = true;
  }
  ENDPOINT("PUT", "chain/{chain-name}", update_chain,
           PATH(oatpp::String, chain_name, "chain-name"),
           BODY_STRING(oatpp::String, chain_data))
  {
    auto janswer = _oja->service_chain(chain_name, chain_data);
    return _oja->jdoc_to_response(janswer);
  }

  ENDPOINT_INFO(create_resource)
  {
    info->summary = "Create/Open a resource for multiple predict calls";
    info->addResponse<Object<dd::DTO::ResourceResponse>>(Status::CODE_201,
                                                         "application/json");
  }
  ENDPOINT("PUT", "resources/{resource-name}", create_resource,
           PATH(oatpp::String, resource_name, "resource-name"),
           BODY_DTO(Object<dd::DTO::Resource>, resource_data))
  {
    try
      {
        return _oja->dto_to_response(
            _oja->create_resource(resource_name, resource_data), 201,
            "Created");
      }
    catch (dd::ResourceBadParamException &e)
      {
        return _oja->response_bad_request_400(e.what());
      }
    catch (dd::ResourceForbiddenException &e)
      {
        return _oja->response_resource_already_exists_1015();
      }
    catch (std::exception &e)
      {
        return _oja->response_internal_error_500(e.what());
      }
    return _oja->response_internal_error_500();
  }

  ENDPOINT_INFO(get_resource)
  {
    info->summary = "Get resource information and status";
    info->addResponse<Object<dd::DTO::ResourceResponse>>(Status::CODE_200,
                                                         "application/json");
  }
  ENDPOINT("GET", "resources/{resource-name}", get_resource,
           PATH(oatpp::String, resource_name, "resource-name"))
  {
    try
      {
        auto res_dto = _oja->get_resource(resource_name);
        return _oja->dto_to_response(res_dto, 200, "OK");
      }
    catch (dd::ResourceNotFoundException &e)
      {
        return _oja->response_not_found_404();
      }
    catch (std::exception &e)
      {
        return _oja->response_internal_error_500(e.what());
      }
    return _oja->response_internal_error_500();
  }

  ENDPOINT_INFO(delete_resource)
  {
    info->summary = "Close and delete an opened resource";
    info->addResponse<Object<dd::DTO::GenericResponse>>(Status::CODE_200,
                                                        "application/json");
  }
  ENDPOINT("DELETE", "resources/{resource-name}", delete_resource,
           PATH(oatpp::String, resource_name, "resource-name"))
  {
    try
      {
        _oja->delete_resource(resource_name);
        return _oja->dto_to_response(dd::DTO::GenericResponse::createShared(),
                                     200, "OK");
      }
    catch (dd::ResourceNotFoundException &e)
      {
        return _oja->response_not_found_404();
      }
    catch (std::exception &e)
      {
        return _oja->response_internal_error_500(e.what());
      }
    return _oja->response_internal_error_500();
  }

  ENDPOINT_INFO(create_stream)
  {
    info->summary = "Create a streaming prediction, ie prediction on "
                    "streaming resource with a streamed output.";
    info->addResponse<Object<dd::DTO::StreamResponse>>(Status::CODE_201,
                                                       "application/json");
  }
  ENDPOINT("PUT", "stream/{stream-name}", create_stream,
           PATH(oatpp::String, stream_name, "stream-name"),
           BODY_DTO(Object<dd::DTO::Stream>, stream_data))
  {
    return createDtoResponse(Status::CODE_201,
                             _oja->create_stream(stream_name, stream_data));
  }

  ENDPOINT_INFO(get_stream_info)
  {
    info->summary = "Get information on running stream";
    info->addResponse<Object<dd::DTO::StreamResponse>>(Status::CODE_200,
                                                       "application/json");
  }
  ENDPOINT("GET", "stream/{stream-name}", get_stream_info,
           PATH(oatpp::String, stream_name, "stream-name"))
  {
    return _oja->dto_to_response(_oja->get_stream_info(stream_name), 200, "");
  }

  ENDPOINT_INFO(delete_stream)
  {
    info->summary = "Stop and remove a running stream";
    info->addResponse<Object<dd::DTO::GenericResponse>>(Status::CODE_200,
                                                        "application/json");
  }
  ENDPOINT("DELETE", "stream/{stream-name}", delete_stream,
           PATH(oatpp::String, stream_name, "stream-name"))
  {
    int status = _oja->delete_stream(stream_name);
    return _oja->dto_to_response(dd::DTO::GenericResponse::createShared(),
                                 status, "");
  }
};

#include OATPP_CODEGEN_END(ApiController)

#endif // CRUD_STATICCONTROLLER_HPP
