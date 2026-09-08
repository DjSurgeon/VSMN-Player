#pragma once

namespace iptv::network {

/**
 * @brief Strictly typed HTTP status codes.
 * 
 * Defines standard HTTP response status codes to avoid using naked integers.
 */
enum class HttpStatusCode : int {
    // 1xx Informational
    Continue = 100,
    SwitchingProtocols = 101,

    // 2xx Success
    Ok = 200,
    Created = 201,
    Accepted = 202,
    NoContent = 204,
    PartialContent = 206, // Critical for HLS/MPEG-TS byte-range requests

    // 3xx Redirection
    MovedPermanently = 301,
    Found = 302,
    SeeOther = 303,
    NotModified = 304,
    TemporaryRedirect = 307,
    PermanentRedirect = 308,

    // 4xx Client Error
    BadRequest = 400,
    Unauthorized = 401,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    RequestTimeout = 408,
    RangeNotSatisfiable = 416,
    TooManyRequests = 429,

    // 5xx Server Error
    InternalServerError = 500,
    NotImplemented = 501,
    BadGateway = 502,
    ServiceUnavailable = 503,
    GatewayTimeout = 504,

    // Custom internal codes
    Unknown = -1
};

} // namespace iptv::network
