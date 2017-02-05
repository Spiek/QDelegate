#ifndef QDELEGATE_H
#define QDELEGATE_H

#include <QObject>
#include <functional>

// Template to normalize T to Value Type
template< typename T>
struct ValueType
{ typedef T type; };

template< typename T>
struct ValueType<T*>
{  typedef typename std::remove_pointer<T>::type type; };

template< typename T>
struct ValueType<T&>
{  typedef typename std::remove_reference<T>::type type; };


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
// Invoker for invoking Object method
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
// Invoker for invoking QObject method
//
template<typename Placeholder, typename Object, typename Method, typename ReturnValue, typename... Args>
class QDelegateInvoker<Placeholder,Object,Method,ReturnValue(Args...)> : public QDelegateInvoker<ReturnValue(Args...)>
{
    public:
        QDelegateInvoker(Object* object, Method method) : object(object), method(method) {
            this->deleteConnection = QObject::connect(object, &QObject::destroyed, [this] { this->object = 0; });
        }
        ~QDelegateInvoker() {
            QObject::disconnect(this->deleteConnection);
        }
        virtual ReturnValue invoke(Args... args) override {
            // if no valid object is present, return default constrcuted value
            if(!this->object) {
                qWarning("QDelegate<QObject>::invoke: object is not valid, return default constructed value");
                return ReturnValue();
            }
            return std::bind(this->method, this->object, args...)();
        }

    private:
        Object* object = 0;
        Method method;
        QMetaObject::Connection deleteConnection;
};


//
// Invoker for invoking QObject method via SIGNAL or SLOT name
//
template<typename ReturnValue, typename... Args>
class QDelegateInvoker<QObject,ReturnValue(Args...)> : public QDelegateInvoker<ReturnValue(Args...)>
{
    public:
        QDelegateInvoker(QObject* object, const char* method, Qt::ConnectionType conType = Qt::DirectConnection) : conType(conType), object(object), method(method) {
            this->deleteConnection = QObject::connect(object, &QObject::destroyed, [this] { this->object = 0; });
        }
        ~QDelegateInvoker() {
            QObject::disconnect(this->deleteConnection);
        }
        virtual ReturnValue invoke(Args... args) override {
            // if no valid object is present, return default constrcuted value
            if(!this->object) {
                qWarning("QDelegate<QObject,const char*>::invoke: object is not valid, return default constructed value");
                return ReturnValue();
            }
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
        QMetaObject::Connection deleteConnection;
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

        QDelegate(ReturnValue (*method)(Args...))  {
            this->invoker = new QDelegateInvoker<ReturnValue (*)(Args...),ReturnValue(Args...)>(method);
        }

        template<typename Object>
        QDelegate(Object* object, ReturnValue (Object::*method)(Args...)) {
            // object check
            if(!object) {
                qWarning("QDelegate<QObject>: object is not valid, object is not invokable!");
                return;
            }

            // if qobject base, use QObject invoker
            if(std::is_base_of<QObject, typename ValueType<Object>::type>::value) {
                this->invoker = new QDelegateInvoker<void,Object,ReturnValue (Object::*)(Args...),ReturnValue(Args...)>(object, method);
            }

            // otherwise use default object->member invoker
            else {
                this->invoker = new QDelegateInvoker<Object,ReturnValue (Object::*)(Args...),ReturnValue(Args...)>(object, method);
            }
        }

        QDelegate(QObject* object, const char* method, Qt::ConnectionType conType = Qt::DirectConnection) {
            // object check
            if(!object) {
                qWarning("QDelegate<QObject>: object is not valid, object is not invokable!");
                return;
            }
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
