#ifndef QDELEGATE_H
#define QDELEGATE_H

#include <QObject>
#include <functional>

//
// Base Invoker
// and Invoker for function objects
//
template <typename...> class QDelegateInvoker;
template<typename ReturnValue, typename... Args>
class QDelegateInvoker<ReturnValue(Args...)>
{
    public:
        QDelegateInvoker() { }
        virtual ~QDelegateInvoker() { }
        QDelegateInvoker(std::function<ReturnValue(Args...)> functor) : functor(functor) { }
        virtual ReturnValue invoke(Args... args) {
            return this->functor(args...);
        }

    private:
        std::function<ReturnValue(Args...)> functor;
};

//
// Invoker for invoking object method
//
template<typename Object, typename Method, typename ReturnValue, typename... Args>
class QDelegateInvoker<Object,Method,ReturnValue(Args...)> : public QDelegateInvoker<ReturnValue(Args...)>
{
    public:
        QDelegateInvoker(Object* object, Method method) : object(object), method(method) { }
        virtual ReturnValue invoke(Args... args) override {
            return std::bind(this->method, this->object, args...)();
        }

    private:
        Object* object = 0;
        Method method;
};

//
// Invoker for invoking QObject method via SIGNAL or SLOT name
//
template<typename ReturnValue, typename... Args>
class QDelegateInvoker<QObject,ReturnValue(Args...)> : public QDelegateInvoker<ReturnValue(Args...)>
{
    public:
        QDelegateInvoker(QObject* object, const char* method, Qt::ConnectionType conType = Qt::DirectConnection) : conType(conType), object(object), method(method) { }
        virtual ReturnValue invoke(Args... args) override {
            QGenericReturnArgument ret;
            object->metaObject()->invokeMethod(this->object, QDelegateInvoker::extractMethodName(method), this->conType, ret, args...);
            return (ReturnValue)ret.data();
        }

    private:
        static QByteArray extractMethodName(const char *member)
        {
            // code from qt source (4.8.2)
            // src: qtimer.cpp
            // line: 354
            const char* bracketPosition = strchr(member, '(');
            if (!bracketPosition || !(member[0] >= '0' && member[0] <= '3')) {
                return QByteArray();
            }
            return QByteArray(member+1, bracketPosition - 1 - member); // extract method name
        }

        Qt::ConnectionType conType;
        QObject* object = 0;
        const char* method;
};

//
// Invoker for invoking static method
//
template<typename Method, typename ReturnValue, typename... Args>
class QDelegateInvoker<Method,ReturnValue(Args...)> : public QDelegateInvoker<ReturnValue(Args...)>
{
    public:
        QDelegateInvoker(Method method) : method(method) { }
        virtual ReturnValue invoke(Args... args) override {
            return std::bind(this->method, args...)();
        }

    private:
        Method method;
};

template <typename...> class QDelegate;
template<typename ReturnValue, typename... Args>
class QDelegate<ReturnValue(Args...)>
{
    public:
        QDelegate(std::function<ReturnValue(Args...)> functor) {
            this->invoker = new QDelegateInvoker<ReturnValue(Args...)>(functor);
        }

        template<typename Method>
        QDelegate(Method method)  {
            this->invoker = new QDelegateInvoker<Method,ReturnValue(Args...)>(method);
        }

        template<typename Object>
        QDelegate(Object* object, ReturnValue (Object::*method)(Args...)) {
           this->invoker = new QDelegateInvoker<Object,ReturnValue (Object::*)(Args...),ReturnValue(Args...)>(object, method);
        }

        QDelegate(QObject* object, const char* method, Qt::ConnectionType conType = Qt::DirectConnection) {
            this->invoker = new QDelegateInvoker<QObject,ReturnValue(Args...)>(object, method, conType);
        }

        ~QDelegate() {
            delete this->invoker;
        }

        ReturnValue invoke(Args... args) {
            // if no invoker is present, return default constrcuted value
            if(!this->invoker) {
                qWarning("QDelegate::invoke: No valid invoker available, return default constrcuted value");
                return ReturnValue();
            }

            // otherwise call invoker
            return this->invoker->invoke(args...);
        }

    private:
         QDelegateInvoker<ReturnValue(Args...)>* invoker = 0;
};

#endif // QDELEGATE_H
