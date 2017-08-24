/*
 * DEBUG: section 93    Adaptation
 */

#include "squid-old.h"
#include "adaptation/Answer.h"
#include "adaptation/Config.h"
#include "adaptation/Iterator.h"
#include "adaptation/Service.h"
#include "adaptation/ServiceFilter.h"
#include "adaptation/ServiceGroups.h"
#include "base/TextException.h"
#include "HttpRequest.h"
#include "HttpReply.h"
#include "HttpMsg.h"


Adaptation::Iterator::Iterator(
    HttpMsg *aMsg, HttpRequest *aCause,
    const ServiceGroupPointer &aGroup):
        AsyncJob("Iterator"),
        Adaptation::Initiate("Iterator"),
        theGroup(aGroup),
        theMsg(HTTPMSGLOCK(aMsg)),
        theCause(aCause ? HTTPMSGLOCK(aCause) : NULL),
        theLauncher(0),
        iterations(0),
        adapted(false)
{
}

Adaptation::Iterator::~Iterator()
{
    assert(!theLauncher);
    HTTPMSGUNLOCK(theMsg);
    HTTPMSGUNLOCK(theCause);
}

void Adaptation::Iterator::start()
{
    Adaptation::Initiate::start();

    thePlan = ServicePlan(theGroup, filter());
    step();
}

void Adaptation::Iterator::step()
{
    ++iterations;
    debugs(93,5, HERE << '#' << iterations << " plan: " << thePlan);

    Must(!theLauncher);

    if (thePlan.exhausted()) { // nothing more to do
        sendAnswer(Answer::Forward(theMsg));
        Must(done());
        return;
    }

    if (iterations > Adaptation::Config::service_iteration_limit) {
        debugs(93,DBG_CRITICAL, "Adaptation iterations limit (" <<
               Adaptation::Config::service_iteration_limit << ") exceeded:\n" <<
               "\tPossible service loop with " <<
               theGroup->kind << " " << theGroup->id << ", plan=" << thePlan);
        throw TexcHere("too many adaptations");
    }

    ServicePointer service = thePlan.current();
    Must(service != NULL);
    debugs(93,5, HERE << "using adaptation service: " << service->cfg().key);

    theLauncher = initiateAdaptation(
                      service->makeXactLauncher(theMsg, theCause));
    Must(initiated(theLauncher));
    Must(!done());
}

void
Adaptation::Iterator::noteAdaptationAnswer(const Answer &answer)
{
    switch (answer.kind) {
    case Answer::akForward:
        handleAdaptedHeader(answer.message);
        break;

    case Answer::akBlock:
        handleAdaptationBlock(answer);
        break;

    case Answer::akError:
        handleAdaptationError(answer.final);
        break;
    }
}

void
Adaptation::Iterator::handleAdaptedHeader(HttpMsg *aMsg)
{
    // set theCause if we switched to request satisfaction mode
    if (!theCause) { // probably sent a request message
        if (dynamic_cast<HttpReply*>(aMsg)) { // we got a response message
            if (HttpRequest *cause = dynamic_cast<HttpRequest*>(theMsg)) {
                // definately sent request, now use it as the cause
                theCause = cause; // moving the lock
                theMsg = 0;
                debugs(93,3, HERE << "in request satisfaction mode");
            }
        }
    }

    Must(aMsg);
    HTTPMSGUNLOCK(theMsg);
    theMsg = HTTPMSGLOCK(aMsg);
    adapted = true;

    clearAdaptation(theLauncher);
    if (!updatePlan(true)) // do not immediatelly advance the new plan
        thePlan.next(filter());
    step();
}

void Adaptation::Iterator::noteInitiatorAborted()
{
    announceInitiatorAbort(theLauncher); // propogate to the transaction
    clearInitiator();
    mustStop("initiator gone");
}

void Adaptation::Iterator::handleAdaptationBlock(const Answer &answer)
{
    debugs(93,5, HERE << "blocked by " << answer);
    clearAdaptation(theLauncher);
    updatePlan(false);
    sendAnswer(answer);
    mustStop("blocked");
}

void Adaptation::Iterator::handleAdaptationError(bool final)
{
    debugs(93,5, HERE << "final: " << final << " plan: " << thePlan);
    clearAdaptation(theLauncher);
    updatePlan(false);

    // can we replace the failed service (group-level bypass)?
    const bool srcIntact = !theMsg->body_pipe ||
                           !theMsg->body_pipe->consumedSize();
    // can we ignore the failure (compute while thePlan is not exhausted)?
    Must(!thePlan.exhausted());
    const bool canIgnore = thePlan.current()->cfg().bypass;
    debugs(85,5, HERE << "flags: " << srcIntact << canIgnore << adapted);

    if (srcIntact) {
        if (thePlan.replacement(filter()) != NULL) {
            debugs(93,3, HERE << "trying a replacement service");
            step();
            return;
        }
    }

    if (canIgnore && srcIntact && adapted) {
        debugs(85,3, HERE << "responding with older adapted msg");
        sendAnswer(Answer::Forward(theMsg));
        mustStop("sent older adapted msg");
        return;
    }

    // caller may recover if we can ignore the error and virgin msg is intact
    const bool useVirgin = canIgnore && !adapted && srcIntact;
    tellQueryAborted(!useVirgin);
    mustStop("group failure");
}

bool Adaptation::Iterator::doneAll() const
{
    return Adaptation::Initiate::doneAll() && thePlan.exhausted();
}

void Adaptation::Iterator::swanSong()
{
    if (theInitiator.set())
        tellQueryAborted(true); // abnormal condition that should not happen

    if (initiated(theLauncher))
        clearAdaptation(theLauncher);

    Adaptation::Initiate::swanSong();
}

bool Adaptation::Iterator::updatePlan(bool adopt)
{
    HttpRequest *r = theCause ? theCause : dynamic_cast<HttpRequest*>(theMsg);
    Must(r);

    Adaptation::History::Pointer ah = r->adaptHistory();
    if (!ah) {
        debugs(85,9, HERE << "no history to store a service-proposed plan");
        return false; // the feature is not enabled or is not triggered
    }

    String services;
    if (!ah->extractNextServices(services)) { // clears history
        debugs(85,9, HERE << "no service-proposed plan received");
        return false; // the service did not provide a new plan
    }

    if (!adopt) {
        debugs(85,3, HERE << "rejecting service-proposed plan");
        return false;
    }

    debugs(85,3, HERE << "retiring old plan: " << thePlan);

    Adaptation::ServiceFilter filter = this->filter();
    DynamicGroupCfg current, future;
    DynamicServiceChain::Split(filter, services, current, future);

    if (!future.empty()) {
        ah->setFutureServices(future);
        debugs(85,3, HERE << "noted future service-proposed plan: " << future);
    }

    // use the current config even if it is empty; we must replace the old plan
    theGroup = new DynamicServiceChain(current, filter); // refcounted
    thePlan = ServicePlan(theGroup, filter);
    debugs(85,3, HERE << "adopted service-proposed plan: " << thePlan);
    return true;
}

Adaptation::ServiceFilter Adaptation::Iterator::filter() const
{
    // the method may differ from theGroup->method due to request satisfaction
    Method method = methodNone;
    // temporary variables, no locking needed
    HttpRequest *req = NULL;
    HttpReply *rep = NULL;

    if (HttpRequest *r = dynamic_cast<HttpRequest*>(theMsg)) {
        method = methodReqmod;
        req = r;
        rep = NULL;
    } else if (HttpReply *theReply = dynamic_cast<HttpReply*>(theMsg)) {
        method = methodRespmod;
        req = theCause;
        rep = theReply;
    } else {
        Must(false); // should not happen
    }

    return ServiceFilter(method, theGroup->point, req, rep);
}

CBDATA_NAMESPACED_CLASS_INIT(Adaptation, Iterator);