////////////////////////////////////////////////////////////////////////////
//
// Copyright 2020 Realm Inc.
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

import Foundation
import RealmSwift

/// A type of rood associated with an `Ingredient`.
enum FoodType: String, CaseIterable {
  case food
  case foodTruck
  case organicFood
  case noFood
  case deliverFood
  case veganFood
  case foodService
  case healthyFood
  case fishFood
  case naturalFood
  case vegetarianFood
  case foodAndWine
  case nonVegetarianFoodSymbol
  case realFoodForMeals
  case healthyFoodCaloriesCalculator
  case mcdonalds
  case noShellfish
  case noCelery
  case noNuts
  case frenchFries
  case noSoy
  case spaghetti
  case ingredients
  case chiliPepper
  case cooker
  case cinnamonRoll
  case zucchini
  case steak
  case tinCan
  case wrap
  case potato
  case groceryBag
  case noApple
  case milkBottle
  case leaf
  case weddingCake
  case tapas
  case wheat
  case rackOfLamb
  case kawaiiBread
  case kawaiiEgg
  case sesame
  case kawaiiTaco
  case orange
  case barley
  case bulog
  case restaurantBuilding
  case salami
  case hotDog
  case kawaiiSushi
  case kawaiiCupcake
  case fridge
  case cottonCandy
  case cherry
  case tomato
  case picnicTable
  case pizza
  case hops
  case watermelon
  case peanuts
  case hazelnut
  case paella
  case kawaiiFrenchFries
  case waiter
  case asparagus
  case garlic
  case noLupines
  case melon
  case paprika
  case restaurant
  case protein
  case toasterOven
  case fiber
  case avocado
  case hamburger
  case soy
  case sushi
  case bento
  case banana
  case iceCreamScoop
  case quesadilla
  case cauliflower
  case pear
  case toaster
  case sprout
  case spamCan
  case vendingMachine
  case apple
  case raspberry
  case sodium
  case noodles
  case kiwi
  case dairy
  case celery
  case halloweenCandy
  case grass
  case snail
  case sashimi
  case palmTree
  case weber
  case corn
  case carbohydrates
  case plum
  case eggplant
  case naan
  case yearOfGoat
  case radish
  case broccoli
  case cucumber
  case sugarCubes
  case sugarCube
  case grill
  case beet
  case brezel

  /// A url associated with an icon for a given food type.
  var imgUrl: String {
    String(format: "https://img.icons8.com/color/48/000000/%@.png", self.rawValue.unicodeScalars.reduce("") {
      if CharacterSet.uppercaseLetters.contains($1) {
        return $0 + "-" + String($1)
      } else {
        return $0 + String($1)
      }
    })
  }
}
