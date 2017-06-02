# QDelegate

QDelegate provides easy to use delegates for:
* Function Objects (Functors)
* Functions
* Class Functions
* Object functions
* QObject Signals/Slots (via SIGNAL/SLOT Macro or as String)

----------

### Standard Invokes

```c++
  // static functions
  QDelegate<int(int, int)>(&MyObject::staticFunction).invoke(10, 1);
  QDelegate<int(int, int)>(&staticFunction).invoke(10, 2);

  // function objects
  QDelegate<int(int, int)>([](int a, int b){ return a + b; }).invoke(10, 3);

  // class objects
  QDelegate<int(int, int)>(MyObject, &MyObject::myfunction).invoke(10, 4);
```

----------

### QObject Invokes

if QDelegate is used to invoke a method on a QObject, there are a few **more** possible invoke options:
```c++
  // invoke a QObject function via functionname as QByteArray or const char*
  QDelegate<int(int, int)>(QObject, "myfunction", Qt::DirectConnection).invoke(10, 5);
  
  // invoke a QObject function via SIGNAL/SLOT macro
  QDelegate<int(int, int)>(QObject, SIGNAL(mysignal(int, int)), Qt::DirectConnection).invoke(10, 7);
  QDelegate<int(int, int)>(QObject, SLOT(myslot(int, int)), Qt::DirectConnection).invoke(10, 6);
```
Additional Notes to QObject QDelegates:
* Function names provided as string are **normalized**, meaning the following function syntaxes are also possible:  
  "function()", "function", "function(int,bool)"
* If you set the Qt::ConnectionType to Qt::QueuedConnection invoke will return a default constructed value of the return value and invoke the function async!
* QDelegate automaticly detects deletions of QObjects and don't invoke them (SEGFAULT protection)

----------

### Multiple Invokes

QDelegate support the call of multiple invokes (with the same function signature):
```c++
  // declare delegate to invoke &MyObject::staticFunction and a Functor
  auto delegate = QDelegate<int(int, int)>(&MyObject::staticFunction);
  delegate.addInvoke([](int a, int b){ return a + b; });
  
  // calls &MyObject::staticFunction and then the Functor and save their return values
  QList<int> returnValues = delegate.invoke(12, 23);
 
  // it's also possible to speed up the invoke massivly by ignoring the return value
  delegate.fastInvoke(12, 23); // returns: void
  
```
The function addInvoke returns a reference to itself, so it's also useable in a chain:
```c++
  // invoke &MyObject::staticFunction and a Functor and save their return values
  QList<int> returnValues = QDelegate<int(int, int)>(&MyObject::staticFunction)
                            .addInvoke([](int a, int b){ return a + b; })
                            .invoke(12, 32);
  
```

----------

### Installation

It's very easy, just add the following to your Project file:
```qmake
include(QDelegate.pri)
```

----------

### Licence
The [QDelegate licence](https://github.com/Spiek/QDelegate/blob/master/LICENCE) is a modified version of the [LGPL](http://www.gnu.org/licenses/lgpl.html) licence, with a static linking exception.
