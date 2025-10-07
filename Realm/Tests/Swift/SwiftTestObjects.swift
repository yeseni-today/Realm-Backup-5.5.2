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
import RealmTestSupport

// MARK: - SwiftRLMStringObject
class SwiftRLMStringObject: RLMObject {
  @objc dynamic var stringCol = ""
}

// MARK: - SwiftRLMBoolObject
class SwiftRLMBoolObject: RLMObject {
  @objc dynamic var boolCol = false
}

// MARK: - SwiftRLMIntObject
class SwiftRLMIntObject: RLMObject {
  @objc dynamic var intCol = 0
}

// MARK: - SwiftRLMLongObject
class SwiftRLMLongObject: RLMObject {
  @objc dynamic var longCol: Int64 = 0
}

// MARK: - SwiftRLMObject
class SwiftRLMObject: RLMObject {
  @objc dynamic var boolCol = false
  @objc dynamic var intCol = 123
  @objc dynamic var floatCol = 1.23 as Float
  @objc dynamic var doubleCol = 12.3
  @objc dynamic var stringCol = "a"
  @objc dynamic var binaryCol = "a".data(using: String.Encoding.utf8)
  @objc dynamic var dateCol = Date(timeIntervalSince1970: 1)
  @objc dynamic var objectCol = SwiftRLMBoolObject()
  @objc dynamic var arrayCol = RLMArray<SwiftRLMBoolObject>(objectClassName: SwiftRLMBoolObject.className())
}

// MARK: - SwiftRLMOptionalObject
class SwiftRLMOptionalObject: RLMObject {
  @objc dynamic var optStringCol: String?
  @objc dynamic var optNSStringCol: NSString?
  @objc dynamic var optBinaryCol: Data?
  @objc dynamic var optDateCol: Date?
  @objc dynamic var optObjectCol: SwiftRLMBoolObject?
}

// MARK: - SwiftRLMPrimitiveArrayObject
class SwiftRLMPrimitiveArrayObject: RLMObject {
  @objc dynamic var stringCol = RLMArray<NSString>(objectType: .string, optional: false)
  @objc dynamic var optStringCol = RLMArray<NSObject>(objectType: .string, optional: true)
  @objc dynamic var dataCol = RLMArray<NSData>(objectType: .data, optional: false)
  @objc dynamic var optDataCol = RLMArray<NSObject>(objectType: .data, optional: true)
  @objc dynamic var dateCol = RLMArray<NSDate>(objectType: .date, optional: false)
  @objc dynamic var optDateCol = RLMArray<NSObject>(objectType: .date, optional: true)
}

// MARK: - SwiftRLMDogObject
class SwiftRLMDogObject: RLMObject {
  @objc dynamic var dogName = ""
}

// MARK: - SwiftRLMOwnerObject
class SwiftRLMOwnerObject: RLMObject {
  @objc dynamic var name = ""
  @objc dynamic var dog: SwiftRLMDogObject? = SwiftRLMDogObject()
}

// MARK: - SwiftRLMAggregateObject
class SwiftRLMAggregateObject: RLMObject {
  @objc dynamic var intCol = 0
  @objc dynamic var floatCol = 0 as Float
  @objc dynamic var doubleCol = 0.0
  @objc dynamic var boolCol = false
  @objc dynamic var dateCol = Date()
}

// MARK: - SwiftRLMAllIntSizesObject
class SwiftRLMAllIntSizesObject: RLMObject {
  @objc dynamic var int8: Int8 = 0
  @objc dynamic var int16: Int16 = 0
  @objc dynamic var int32: Int32 = 0
  @objc dynamic var int64: Int64 = 0
}

// MARK: - SwiftRLMEmployeeObject
class SwiftRLMEmployeeObject: RLMObject {
  @objc dynamic var name = ""
  @objc dynamic var age = 0
  @objc dynamic var hired = false
}

// MARK: - SwiftRLMCompanyObject
class SwiftRLMCompanyObject: RLMObject {
  @objc dynamic var employees = RLMArray<SwiftRLMEmployeeObject>(objectClassName: SwiftRLMEmployeeObject.className())
}

// MARK: - SwiftRLMArrayPropertyObject
class SwiftRLMArrayPropertyObject: RLMObject {
  @objc dynamic var name = ""
  @objc dynamic var array = RLMArray<SwiftRLMStringObject>(objectClassName: SwiftRLMStringObject.className())
  @objc dynamic var intArray = RLMArray<SwiftRLMIntObject>(objectClassName: SwiftRLMIntObject.className())
}

// MARK: - SwiftRLMDynamicObject
class SwiftRLMDynamicObject: RLMObject {
  @objc dynamic var stringCol = "a"
  @objc dynamic var intCol = 0
}

// MARK: - SwiftRLMUTF8Object
class SwiftRLMUTF8Object: RLMObject {
  @objc dynamic var 柱колоéнǢкƱаم👍 = "值значен™👍☞⎠‱௹♣︎☐▼❒∑⨌⧭иеمرحبا"
}

// MARK: - SwiftRLMIgnoredPropertiesObject
class SwiftRLMIgnoredPropertiesObject: RLMObject {
  @objc dynamic var name = ""
  @objc dynamic var age = 0
  @objc dynamic var runtimeProperty: AnyObject?
  @objc dynamic var readOnlyProperty: Int { return 0 }

  override class func ignoredProperties() -> [String]? {
    return ["runtimeProperty"]
  }
}

// MARK: - SwiftRLMPrimaryStringObject
class SwiftRLMPrimaryStringObject: RLMObject {
  @objc dynamic var stringCol = ""
  @objc dynamic var intCol = 0

  override class func primaryKey() -> String {
    return "stringCol"
  }
}

// MARK: - SwiftRLMLinkSourceObject
class SwiftRLMLinkSourceObject: RLMObject {
  @objc dynamic var id = 0
  @objc dynamic var link: SwiftRLMLinkTargetObject?
}

// MARK: - SwiftRLMLinkTargetObject
class SwiftRLMLinkTargetObject: RLMObject {
  @objc dynamic var id = 0
  @objc dynamic var backlinks: RLMLinkingObjects<SwiftRLMLinkSourceObject>?

  override class func linkingObjectsProperties() -> [String: RLMPropertyDescriptor] {
    return ["backlinks": RLMPropertyDescriptor(with: SwiftRLMLinkSourceObject.self, propertyName: "link")]
  }
}

// MARK: - SwiftRLMLazyVarObject
class SwiftRLMLazyVarObject: RLMObject {
  @objc dynamic lazy var lazyProperty: String = "hello world"
}

// MARK: - SwiftRLMIgnoredLazyVarObject
class SwiftRLMIgnoredLazyVarObject: RLMObject {
  @objc dynamic var id = 0
  @objc dynamic lazy var ignoredVar: String = "hello world"
  override class func ignoredProperties() -> [String] { return ["ignoredVar"] }
}

// MARK: - SwiftRLMObjectiveCTypesObject
class SwiftRLMObjectiveCTypesObject: RLMObject {
  @objc dynamic var stringCol: NSString?
  @objc dynamic var dateCol: NSDate?
  @objc dynamic var dataCol: NSData?
  @objc dynamic var numCol: NSNumber? = 0
}
