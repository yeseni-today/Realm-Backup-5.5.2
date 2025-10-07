////////////////////////////////////////////////////////////////////////////
//
// Copyright 2014 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

import Realm
import Foundation
import RealmSwift

// MARK: - SwiftStringObject
class SwiftStringObject: Object {
  @objc dynamic var stringCol = ""
}

// MARK: - SwiftBoolObject
class SwiftBoolObject: Object {
  @objc dynamic var boolCol = false
}

// MARK: - SwiftIntObject
class SwiftIntObject: Object {
  @objc dynamic var intCol = 0
}

// MARK: - SwiftLongObject
class SwiftLongObject: Object {
  @objc dynamic var longCol: Int64 = 0
}

// MARK: - IntEnum
@objc enum IntEnum: Int, RealmEnum {
  case value1 = 1
  case value2 = 3
}

// MARK: - SwiftObject
class SwiftObject: Object {
  @objc dynamic var boolCol = false
  @objc dynamic var intCol = 123
  @objc dynamic var intEnumCol = IntEnum.value1
  @objc dynamic var floatCol = 1.23 as Float
  @objc dynamic var doubleCol = 12.3
  @objc dynamic var stringCol = "a"
  @objc dynamic var binaryCol = "a".data(using: String.Encoding.utf8)!
  @objc dynamic var dateCol = Date(timeIntervalSince1970: 1)
  @objc dynamic var objectCol: SwiftBoolObject? = SwiftBoolObject()
  let arrayCol = List<SwiftBoolObject>()

  class func defaultValues() -> [String: Any] {
    return [
      "boolCol": false,
      "intCol": 123,
      "floatCol": 1.23 as Float,
      "doubleCol": 12.3,
      "stringCol": "a",
      "binaryCol": "a".data(using: String.Encoding.utf8)!,
      "dateCol": Date(timeIntervalSince1970: 1),
      "objectCol": [false],
      "arrayCol": [],
    ]
  }
}

// MARK: - SwiftOptionalObject
class SwiftOptionalObject: Object {
  @objc dynamic var optNSStringCol: NSString?
  @objc dynamic var optStringCol: String?
  @objc dynamic var optBinaryCol: Data?
  @objc dynamic var optDateCol: Date?
  let optIntCol = RealmOptional<Int>()
  let optInt8Col = RealmOptional<Int8>()
  let optInt16Col = RealmOptional<Int16>()
  let optInt32Col = RealmOptional<Int32>()
  let optInt64Col = RealmOptional<Int64>()
  let optFloatCol = RealmOptional<Float>()
  let optDoubleCol = RealmOptional<Double>()
  let optBoolCol = RealmOptional<Bool>()
  let optEnumCol = RealmOptional<IntEnum>()
  @objc dynamic var optObjectCol: SwiftBoolObject?
}

// MARK: - SwiftOptionalPrimaryObject
class SwiftOptionalPrimaryObject: SwiftOptionalObject {
  let id = RealmOptional<Int>()

  override class func primaryKey() -> String? { return "id" }
}

// MARK: - SwiftListObject
class SwiftListObject: Object {
  let int = List<Int>()
  let int8 = List<Int8>()
  let int16 = List<Int16>()
  let int32 = List<Int32>()
  let int64 = List<Int64>()
  let float = List<Float>()
  let double = List<Double>()
  let string = List<String>()
  let data = List<Data>()
  let date = List<Date>()

  let intOpt = List<Int?>()
  let int8Opt = List<Int8?>()
  let int16Opt = List<Int16?>()
  let int32Opt = List<Int32?>()
  let int64Opt = List<Int64?>()
  let floatOpt = List<Float?>()
  let doubleOpt = List<Double?>()
  let stringOpt = List<String?>()
  let dataOpt = List<Data?>()
  let dateOpt = List<Date?>()
}

// MARK: - SwiftImplicitlyUnwrappedOptionalObject
class SwiftImplicitlyUnwrappedOptionalObject: Object {
  @objc dynamic var optNSStringCol: NSString!
  @objc dynamic var optStringCol: String!
  @objc dynamic var optBinaryCol: Data!
  @objc dynamic var optDateCol: Date!
  @objc dynamic var optObjectCol: SwiftBoolObject!
}

// MARK: - SwiftOptionalDefaultValuesObject
class SwiftOptionalDefaultValuesObject: Object {
  @objc dynamic var optNSStringCol: NSString? = "A"
  @objc dynamic var optStringCol: String? = "B"
  @objc dynamic var optBinaryCol: Data? = "C".data(using: String.Encoding.utf8)! as Data
  @objc dynamic var optDateCol: Date? = Date(timeIntervalSince1970: 10)
  let optIntCol = RealmOptional<Int>(1)
  let optInt8Col = RealmOptional<Int8>(1)
  let optInt16Col = RealmOptional<Int16>(1)
  let optInt32Col = RealmOptional<Int32>(1)
  let optInt64Col = RealmOptional<Int64>(1)
  let optFloatCol = RealmOptional<Float>(2.2)
  let optDoubleCol = RealmOptional<Double>(3.3)
  let optBoolCol = RealmOptional<Bool>(true)
  @objc dynamic var optObjectCol: SwiftBoolObject? = SwiftBoolObject(value: [true])
  //    let arrayCol = List<SwiftBoolObject?>()

  class func defaultValues() -> [String: Any] {
    return [
      "optNSStringCol": "A",
      "optStringCol": "B",
      "optBinaryCol": "C".data(using: String.Encoding.utf8)!,
      "optDateCol": Date(timeIntervalSince1970: 10),
      "optIntCol": 1,
      "optInt8Col": 1,
      "optInt16Col": 1,
      "optInt32Col": 1,
      "optInt64Col": 1,
      "optFloatCol": 2.2 as Float,
      "optDoubleCol": 3.3,
      "optBoolCol": true,
    ]
  }
}

// MARK: - SwiftOptionalIgnoredPropertiesObject
class SwiftOptionalIgnoredPropertiesObject: Object {
  @objc dynamic var id = 0

  @objc dynamic var optNSStringCol: NSString? = "A"
  @objc dynamic var optStringCol: String? = "B"
  @objc dynamic var optBinaryCol: Data? = "C".data(using: String.Encoding.utf8)! as Data
  @objc dynamic var optDateCol: Date? = Date(timeIntervalSince1970: 10)
  @objc dynamic var optObjectCol: SwiftBoolObject? = SwiftBoolObject(value: [true])

  override class func ignoredProperties() -> [String] {
    return [
      "optNSStringCol",
      "optStringCol",
      "optBinaryCol",
      "optDateCol",
      "optObjectCol",
    ]
  }
}

// MARK: - SwiftDogObject
class SwiftDogObject: Object {
  @objc dynamic var dogName = ""
  let owners = LinkingObjects(fromType: SwiftOwnerObject.self, property: "dog")
}

// MARK: - SwiftOwnerObject
class SwiftOwnerObject: Object {
  @objc dynamic var name = ""
  @objc dynamic var dog: SwiftDogObject? = SwiftDogObject()
}

// MARK: - SwiftAggregateObject
class SwiftAggregateObject: Object {
  @objc dynamic var intCol = 0
  @objc dynamic var floatCol = 0 as Float
  @objc dynamic var doubleCol = 0.0
  @objc dynamic var boolCol = false
  @objc dynamic var dateCol = Date()
  @objc dynamic var trueCol = true
  let stringListCol = List<SwiftStringObject>()
}

// MARK: - SwiftAllIntSizesObject
class SwiftAllIntSizesObject: Object {
  @objc dynamic var int8: Int8 = 0
  @objc dynamic var int16: Int16 = 0
  @objc dynamic var int32: Int32 = 0
  @objc dynamic var int64: Int64 = 0
}

// MARK: - SwiftEmployeeObject
class SwiftEmployeeObject: Object {
  @objc dynamic var name = ""
  @objc dynamic var age = 0
  @objc dynamic var hired = false
}

// MARK: - SwiftCompanyObject
class SwiftCompanyObject: Object {
  let employees = List<SwiftEmployeeObject>()
}

// MARK: - SwiftArrayPropertyObject
class SwiftArrayPropertyObject: Object {
  @objc dynamic var name = ""
  let array = List<SwiftStringObject>()
  let intArray = List<SwiftIntObject>()
}

// MARK: - SwiftDoubleListOfSwiftObject
class SwiftDoubleListOfSwiftObject: Object {
  let array = List<SwiftListOfSwiftObject>()
}

// MARK: - SwiftListOfSwiftObject
class SwiftListOfSwiftObject: Object {
  let array = List<SwiftObject>()
}

// MARK: - SwiftListOfSwiftOptionalObject
class SwiftListOfSwiftOptionalObject: Object {
  let array = List<SwiftOptionalObject>()
}

// MARK: - SwiftArrayPropertySubclassObject
class SwiftArrayPropertySubclassObject: SwiftArrayPropertyObject {
  let boolArray = List<SwiftBoolObject>()
}

// MARK: - SwiftLinkToPrimaryStringObject
class SwiftLinkToPrimaryStringObject: Object {
  @objc dynamic var pk = ""
  @objc dynamic var object: SwiftPrimaryStringObject?
  let objects = List<SwiftPrimaryStringObject>()

  override class func primaryKey() -> String? {
    return "pk"
  }
}

// MARK: - SwiftUTF8Object
class SwiftUTF8Object: Object {
  // swiftlint:disable:next identifier_name
  @objc dynamic var 柱колоéнǢкƱаم👍 = "值значен™👍☞⎠‱௹♣︎☐▼❒∑⨌⧭иеمرحبا"
}

// MARK: - SwiftIgnoredPropertiesObject
class SwiftIgnoredPropertiesObject: Object {
  @objc dynamic var name = ""
  @objc dynamic var age = 0
  @objc dynamic var runtimeProperty: AnyObject?
  @objc dynamic var runtimeDefaultProperty = "property"
  @objc dynamic var readOnlyProperty: Int { return 0 }

  override class func ignoredProperties() -> [String] {
    return ["runtimeProperty", "runtimeDefaultProperty"]
  }
}

// MARK: - SwiftRecursiveObject
class SwiftRecursiveObject: Object {
  let objects = List<SwiftRecursiveObject>()
}

// MARK: - SwiftPrimaryKeyObjectType
protocol SwiftPrimaryKeyObjectType {
  associatedtype PrimaryKey
  static func primaryKey() -> String?
}

// MARK: - SwiftPrimaryStringObject
class SwiftPrimaryStringObject: Object, SwiftPrimaryKeyObjectType {
  @objc dynamic var stringCol = ""
  @objc dynamic var intCol = 0

  typealias PrimaryKey = String
  override class func primaryKey() -> String? {
    return "stringCol"
  }
}

// MARK: - SwiftPrimaryOptionalStringObject
class SwiftPrimaryOptionalStringObject: Object, SwiftPrimaryKeyObjectType {
  @objc dynamic var stringCol: String? = ""
  @objc dynamic var intCol = 0

  typealias PrimaryKey = String?
  override class func primaryKey() -> String? {
    return "stringCol"
  }
}

// MARK: - SwiftPrimaryIntObject
class SwiftPrimaryIntObject: Object, SwiftPrimaryKeyObjectType {
  @objc dynamic var stringCol = ""
  @objc dynamic var intCol = 0

  typealias PrimaryKey = Int
  override class func primaryKey() -> String? {
    return "intCol"
  }
}

// MARK: - SwiftPrimaryOptionalIntObject
class SwiftPrimaryOptionalIntObject: Object, SwiftPrimaryKeyObjectType {
  @objc dynamic var stringCol = ""
  let intCol = RealmOptional<Int>()

  typealias PrimaryKey = RealmOptional<Int>
  override class func primaryKey() -> String? {
    return "intCol"
  }
}

// MARK: - SwiftPrimaryInt8Object
class SwiftPrimaryInt8Object: Object, SwiftPrimaryKeyObjectType {
  @objc dynamic var stringCol = ""
  @objc dynamic var int8Col: Int8 = 0

  typealias PrimaryKey = Int8
  override class func primaryKey() -> String? {
    return "int8Col"
  }
}

// MARK: - SwiftPrimaryOptionalInt8Object
class SwiftPrimaryOptionalInt8Object: Object, SwiftPrimaryKeyObjectType {
  @objc dynamic var stringCol = ""
  let int8Col = RealmOptional<Int8>()

  typealias PrimaryKey = RealmOptional<Int8>
  override class func primaryKey() -> String? {
    return "int8Col"
  }
}

// MARK: - SwiftPrimaryInt16Object
class SwiftPrimaryInt16Object: Object, SwiftPrimaryKeyObjectType {
  @objc dynamic var stringCol = ""
  @objc dynamic var int16Col: Int16 = 0

  typealias PrimaryKey = Int16
  override class func primaryKey() -> String? {
    return "int16Col"
  }
}

// MARK: - SwiftPrimaryOptionalInt16Object
class SwiftPrimaryOptionalInt16Object: Object, SwiftPrimaryKeyObjectType {
  @objc dynamic var stringCol = ""
  let int16Col = RealmOptional<Int16>()

  typealias PrimaryKey = RealmOptional<Int16>
  override class func primaryKey() -> String? {
    return "int16Col"
  }
}

// MARK: - SwiftPrimaryInt32Object
class SwiftPrimaryInt32Object: Object, SwiftPrimaryKeyObjectType {
  @objc dynamic var stringCol = ""
  @objc dynamic var int32Col: Int32 = 0

  typealias PrimaryKey = Int32
  override class func primaryKey() -> String? {
    return "int32Col"
  }
}

// MARK: - SwiftPrimaryOptionalInt32Object
class SwiftPrimaryOptionalInt32Object: Object, SwiftPrimaryKeyObjectType {
  @objc dynamic var stringCol = ""
  let int32Col = RealmOptional<Int32>()

  typealias PrimaryKey = RealmOptional<Int32>
  override class func primaryKey() -> String? {
    return "int32Col"
  }
}

// MARK: - SwiftPrimaryInt64Object
class SwiftPrimaryInt64Object: Object, SwiftPrimaryKeyObjectType {
  @objc dynamic var stringCol = ""
  @objc dynamic var int64Col: Int64 = 0

  typealias PrimaryKey = Int64
  override class func primaryKey() -> String? {
    return "int64Col"
  }
}

// MARK: - SwiftPrimaryOptionalInt64Object
class SwiftPrimaryOptionalInt64Object: Object, SwiftPrimaryKeyObjectType {
  @objc dynamic var stringCol = ""
  let int64Col = RealmOptional<Int64>()

  typealias PrimaryKey = RealmOptional<Int64>
  override class func primaryKey() -> String? {
    return "int64Col"
  }
}

// MARK: - SwiftIndexedPropertiesObject
class SwiftIndexedPropertiesObject: Object {
  @objc dynamic var stringCol = ""
  @objc dynamic var intCol = 0
  @objc dynamic var int8Col: Int8 = 0
  @objc dynamic var int16Col: Int16 = 0
  @objc dynamic var int32Col: Int32 = 0
  @objc dynamic var int64Col: Int64 = 0
  @objc dynamic var boolCol = false
  @objc dynamic var dateCol = Date()

  @objc dynamic var floatCol: Float = 0.0
  @objc dynamic var doubleCol: Double = 0.0
  @objc dynamic var dataCol = Data()

  override class func indexedProperties() -> [String] {
    return ["stringCol", "intCol", "int8Col", "int16Col", "int32Col", "int64Col", "boolCol", "dateCol"]
  }
}

// MARK: - SwiftIndexedOptionalPropertiesObject
class SwiftIndexedOptionalPropertiesObject: Object {
  @objc dynamic var optionalStringCol: String? = ""
  let optionalIntCol = RealmOptional<Int>()
  let optionalInt8Col = RealmOptional<Int8>()
  let optionalInt16Col = RealmOptional<Int16>()
  let optionalInt32Col = RealmOptional<Int32>()
  let optionalInt64Col = RealmOptional<Int64>()
  let optionalBoolCol = RealmOptional<Bool>()
  @objc dynamic var optionalDateCol: Date? = Date()

  let optionalFloatCol = RealmOptional<Float>()
  let optionalDoubleCol = RealmOptional<Double>()
  @objc dynamic var optionalDataCol: Data? = Data()

  override class func indexedProperties() -> [String] {
    return ["optionalStringCol", "optionalIntCol", "optionalInt8Col", "optionalInt16Col",
            "optionalInt32Col", "optionalInt64Col", "optionalBoolCol", "optionalDateCol"]
  }
}

// MARK: - SwiftCustomInitializerObject
class SwiftCustomInitializerObject: Object {
  @objc dynamic var stringCol: String

  init(stringVal: String) {
    stringCol = stringVal
    super.init()
  }

  required init() {
    stringCol = ""
    super.init()
  }
}

// MARK: - SwiftConvenienceInitializerObject
class SwiftConvenienceInitializerObject: Object {
  @objc dynamic var stringCol = ""

  convenience init(stringCol: String) {
    self.init()
    self.stringCol = stringCol
  }
}

// MARK: - SwiftObjectiveCTypesObject
class SwiftObjectiveCTypesObject: Object {
  @objc dynamic var stringCol: NSString?
  @objc dynamic var dateCol: NSDate?
  @objc dynamic var dataCol: NSData?
}

// MARK: - SwiftComputedPropertyNotIgnoredObject
class SwiftComputedPropertyNotIgnoredObject: Object {
  // swiftlint:disable:next identifier_name
  @objc dynamic var _urlBacking = ""

  /// Dynamic; no ivar
  @objc dynamic var dynamicURL: URL? {
    get {
      return URL(string: _urlBacking)
    }
    set {
      _urlBacking = newValue?.absoluteString ?? ""
    }
  }

  /// Non-dynamic; no ivar
  var url: URL? {
    get {
      return URL(string: _urlBacking)
    }
    set {
      _urlBacking = newValue?.absoluteString ?? ""
    }
  }
}

// MARK: - SwiftObjcRenamedObject
@objc(SwiftObjcRenamedObject)
class SwiftObjcRenamedObject: Object {
  @objc dynamic var stringCol = ""
}

// MARK: - SwiftObjcArbitrarilyRenamedObject
@objc(SwiftObjcRenamedObjectWithTotallyDifferentName)
class SwiftObjcArbitrarilyRenamedObject: Object {
  @objc dynamic var boolCol = false
}

// MARK: - SwiftCircleObject
class SwiftCircleObject: Object {
  @objc dynamic var obj: SwiftCircleObject?
  let array = List<SwiftCircleObject>()
}

// MARK: - SwiftGenericPropsOrderingParent
/// Exists to serve as a superclass to `SwiftGenericPropsOrderingObject`
class SwiftGenericPropsOrderingParent: Object {
  var implicitlyIgnoredComputedProperty: Int { return 0 }
  let implicitlyIgnoredReadOnlyProperty: Int = 1
  let parentFirstList = List<SwiftIntObject>()
  @objc dynamic var parentFirstNumber = 0
  func parentFunction() -> Int { return parentFirstNumber + 1 }
  @objc dynamic var parentSecondNumber = 1
  var parentComputedProp: String { return "hello world" }
}

// MARK: - SwiftGenericPropsOrderingObject
/// Used to verify that Swift properties (generic and otherwise) are detected properly and
/// added to the schema in the correct order.
class SwiftGenericPropsOrderingObject: SwiftGenericPropsOrderingParent {
  func myFunction() -> Int { return firstNumber + secondNumber + thirdNumber }
  @objc dynamic var dynamicComputed: Int { return 999 }
  var firstIgnored = 999
  @objc dynamic var dynamicIgnored = 999
  @objc dynamic var firstNumber = 0 // Managed property
  class func myClassFunction(x: Int, y: Int) -> Int { return x + y }
  var secondIgnored = 999
  lazy var lazyIgnored = 999
  let firstArray = List<SwiftStringObject>() // Managed property
  @objc dynamic var secondNumber = 0 // Managed property
  var computedProp: String { return "\(firstNumber), \(secondNumber), and \(thirdNumber)" }
  let secondArray = List<SwiftStringObject>() // Managed property
  override class func ignoredProperties() -> [String] {
    return ["firstIgnored", "dynamicIgnored", "secondIgnored", "thirdIgnored", "lazyIgnored", "dynamicLazyIgnored"]
  }

  let firstOptionalNumber = RealmOptional<Int>() // Managed property
  var thirdIgnored = 999
  @objc dynamic lazy var dynamicLazyIgnored = 999
  let firstLinking = LinkingObjects(fromType: SwiftGenericPropsOrderingHelper.self, property: "first")
  let secondLinking = LinkingObjects(fromType: SwiftGenericPropsOrderingHelper.self, property: "second")
  @objc dynamic var thirdNumber = 0 // Managed property
  let secondOptionalNumber = RealmOptional<Int>() // Managed property
}

// MARK: - SwiftGenericPropsOrderingHelper
/// Only exists to allow linking object properties on `SwiftGenericPropsNotLastObject`.
class SwiftGenericPropsOrderingHelper: Object {
  @objc dynamic var first: SwiftGenericPropsOrderingObject?
  @objc dynamic var second: SwiftGenericPropsOrderingObject?
}

// MARK: - SwiftRenamedProperties1
class SwiftRenamedProperties1: Object {
  @objc dynamic var propA = 0
  @objc dynamic var propB = ""
  let linking1 = LinkingObjects(fromType: LinkToSwiftRenamedProperties1.self, property: "linkA")
  let linking2 = LinkingObjects(fromType: LinkToSwiftRenamedProperties2.self, property: "linkD")

  override class func _realmObjectName() -> String { return "Swift Renamed Properties" }
  override class func _realmColumnNames() -> [String: String] {
    return ["propA": "prop 1", "propB": "prop 2"]
  }
}

// MARK: - SwiftRenamedProperties2
class SwiftRenamedProperties2: Object {
  @objc dynamic var propC = 0
  @objc dynamic var propD = ""
  let linking1 = LinkingObjects(fromType: LinkToSwiftRenamedProperties1.self, property: "linkA")
  let linking2 = LinkingObjects(fromType: LinkToSwiftRenamedProperties2.self, property: "linkD")

  override class func _realmObjectName() -> String { return "Swift Renamed Properties" }
  override class func _realmColumnNames() -> [String: String] {
    return ["propC": "prop 1", "propD": "prop 2"]
  }
}

// MARK: - LinkToSwiftRenamedProperties1
class LinkToSwiftRenamedProperties1: Object {
  @objc dynamic var linkA: SwiftRenamedProperties1?
  @objc dynamic var linkB: SwiftRenamedProperties2?
  let array1 = List<SwiftRenamedProperties1>()

  override class func _realmObjectName() -> String { return "Link To Swift Renamed Properties" }
  override class func _realmColumnNames() -> [String: String] {
    return ["linkA": "link 1", "linkB": "link 2", "array1": "array"]
  }
}

// MARK: - LinkToSwiftRenamedProperties2
class LinkToSwiftRenamedProperties2: Object {
  @objc dynamic var linkC: SwiftRenamedProperties1?
  @objc dynamic var linkD: SwiftRenamedProperties2?
  let array2 = List<SwiftRenamedProperties2>()

  override class func _realmObjectName() -> String { return "Link To Swift Renamed Properties" }
  override class func _realmColumnNames() -> [String: String] {
    return ["linkC": "link 1", "linkD": "link 2", "array2": "array"]
  }
}
