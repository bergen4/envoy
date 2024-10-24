#include "source/extensions/tracers/skywalking/tracer.h"

#include <string>

#if defined(HIGRESS)
#include "source/common/common/base64.h"
#endif

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace SkyWalking {

namespace {
static constexpr absl::string_view StatusCodeTag = "status_code";
static constexpr absl::string_view UrlTag = "url";
} // namespace

const Http::LowerCaseString& skywalkingPropagationHeaderKey() {
  CONSTRUCT_ON_FIRST_USE(Http::LowerCaseString, "sw8");
}

#if defined(HIGRESS)
const Http::LowerCaseString& skywalkingPropagationHeaderKeyTraceId() {
  CONSTRUCT_ON_FIRST_USE(Http::LowerCaseString, "sw8-traceid");
}
#endif

void Span::setTag(absl::string_view name, absl::string_view value) {
  if (name == Tracing::Tags::get().HttpUrl) {
    span_entity_->addTag(UrlTag.data(), std::string(value));
  } else if (name == Tracing::Tags::get().HttpStatusCode) {
    span_entity_->addTag(StatusCodeTag.data(), std::string(value));
  } else if (name == Tracing::Tags::get().Error) {
    span_entity_->setErrorStatus();
    span_entity_->addTag(std::string(name), std::string(value));
  } else if (name == Tracing::Tags::get().PeerAddress) {
    span_entity_->setPeer(std::string(value));
  } else {
    span_entity_->addTag(std::string(name), std::string(value));
  }
}

void Span::setSampled(bool do_sample) {
  // Sampling status is always true on SkyWalking. But with disabling skip_analysis,
  // this span can't be analyzed.
  if (!do_sample) {
    span_entity_->setSkipAnalysis();
  }
}

void Span::log(SystemTime, const std::string& event) { span_entity_->addLog(EMPTY_STRING, event); }

void Span::finishSpan() {
  span_entity_->endSpan();
  parent_tracer_.sendSegment(tracing_context_);
}

void Span::injectContext(Tracing::TraceContext& trace_context,
                         const Upstream::HostDescriptionConstSharedPtr& upstream) {
  absl::string_view remote_address =
      upstream != nullptr ? upstream->address()->asStringView() : trace_context.host();

  auto sw8_header =
      tracing_context_->createSW8HeaderValue({remote_address.data(), remote_address.size()});
  if (sw8_header.has_value()) {
    trace_context.setByReferenceKey(skywalkingPropagationHeaderKey(), sw8_header.value());
#if defined(HIGRESS)
    std::vector<std::string> result = absl::StrSplit(sw8_header.value(), '-');
    std::string sw8_trace_id = "";
    if (result.size() > 1) {
      sw8_trace_id = Base64::decode(result[1]); 
    }
    trace_context.setByReferenceKey(skywalkingPropagationHeaderKeyTraceId(), sw8_trace_id);
#endif
    // Rewrite operation name with latest upstream request path for the EXIT span.
    absl::string_view upstream_request_path = trace_context.path();
    span_entity_->setOperationName({upstream_request_path.data(), upstream_request_path.size()});
  }
}

Tracing::SpanPtr Span::spawnChild(const Tracing::Config&, const std::string&, SystemTime) {
  // Reuse operation name of parent span by default.
  return std::make_unique<Span>(span_entity_->operationName(), span_entity_->spanLayer(), *this,
                                tracing_context_, parent_tracer_);
}

Tracer::Tracer(TraceSegmentReporterPtr reporter) : reporter_(std::move(reporter)) {}

void Tracer::sendSegment(TracingContextPtr segment_context) {
  ASSERT(reporter_);
  if (segment_context->readyToSend()) {
    reporter_->report(std::move(segment_context));
  }
}

Tracing::SpanPtr Tracer::startSpan(absl::string_view name, absl::string_view protocol,
                                   TracingContextPtr tracing_context) {
  return std::make_unique<Span>(name, protocol, tracing_context, *this);
}
} // namespace SkyWalking
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
