#pragma once

#include "physiology/Anatomy.hpp"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace colony::injury {

enum class Part : int { Leg = 0, Arm = 1, Torso = 2, Head = 3 };
enum class Severity { Mild, Serious, Critical };

inline constexpr double seriousDamage = 12;
inline constexpr double criticalDamage = 24;
inline constexpr double maximumDamage = 256;

struct ProfilePart {
    Part id;
    std::string name;
    double hitUpperBound;
    bool affectsMovement = false;
    bool affectsArmFunction = false;
};

struct BloodParameters {
    double capacityMl = 5000;
    double incapacitationFraction = .5;
    double regenerationFractionPerSecond = .001;
    double lossPerOpenDamage = .5;
};

struct TraumaParameters {
    int incapacitating = 4;
    int lethal = 6;
    double mildWoundDamage = 4;
    double seriousWoundDamage = seriousDamage;
    double criticalWoundDamage = criticalDamage;
    double injuryChance = .65;
    std::optional<int> movementTrauma;
    std::optional<Severity> movementWoundSeverity;
};

struct RecoveryParameters {
    // Mild, serious, critical, in that order.
    std::array<double,3> bedRestSeconds{30,90,180};
    double untreatedRate = .1;
    double treatedRate = 1;
};

struct ProfileDefinition {
    std::string id;
    std::vector<ProfilePart> parts;
    BloodParameters blood;
    TraumaParameters trauma;
    RecoveryParameters recovery;
};

// A validated immutable definition shared by all organisms using this profile.
class Profile {
public:
    explicit Profile(ProfileDefinition definition);
    Profile(const Profile&) = default;
    Profile& operator=(const Profile&) = default;

    [[nodiscard]] const ProfileDefinition& definition() const;
    [[nodiscard]] const physiology::Anatomy<Part>& anatomy() const;
    [[nodiscard]] const ProfilePart* part(Part id) const;

private:
    struct Data;
    std::shared_ptr<const Data> data_;
};

} // namespace colony::injury
