#ifndef RENDER_PASS_ATMOSPHERE_PROPERTIES_H
#define RENDER_PASS_ATMOSPHERE_PROPERTIES_H

#include "../../core/math/vector.h"

namespace fantasy::constant
{
	struct AtmosphereProperties
	{
		float3 raylegh_scatter = { 5.802f, 13.558f, 33.1f };     // (um^-1)
		float raylegh_density = 8.0f;   // (km)

		float mie_scatter = 3.996f; // (um^-1)
		float mie_density = 1.2f;   // (km)
		float mie_absorb = 4.4f;    // (um^-1)
		float mie_asymmetry = 0.8f;

		float3 ozone_absorb = { 0.65f, 1.881f, 0.085f };  // (um^-1)
		float ozone_center_height = 25.0f;   // (km)

		float ozone_thickness = 30.0f;      // (km)
		float planet_radius = 6360.0f;      // (km)
		float atmosphere_radius = 6460.0f;  // (km)
		float pad = 0.0f;

		AtmosphereProperties to_standard_unit()
		{
			return AtmosphereProperties{
				.raylegh_scatter = raylegh_scatter * 1e-6f,
				.raylegh_density = raylegh_density * 1e3f,
				.mie_scatter = mie_scatter * 1e-6f,
				.mie_density = mie_density * 1e3f,
				.mie_absorb = mie_absorb * 1e-6f,
				.mie_asymmetry = mie_asymmetry,
				.ozone_absorb = ozone_absorb * 1e-6f,
				.ozone_center_height = ozone_center_height * 1e3f,
				.ozone_thickness = ozone_thickness * 1e3f,
				.planet_radius = planet_radius * 1e3f,
				.atmosphere_radius = atmosphere_radius * 1e3f
			};
		}

		bool operator==(const AtmosphereProperties& other) const
		{
			return 	raylegh_scatter == other.raylegh_scatter &&
				raylegh_density == other.raylegh_density &&
				mie_scatter == other.mie_scatter &&
				mie_density == other.mie_density &&
				mie_absorb == other.mie_absorb &&
				mie_asymmetry == other.mie_asymmetry &&
				ozone_absorb == other.ozone_absorb &&
				ozone_center_height == other.ozone_center_height &&
				ozone_thickness == other.ozone_thickness &&
				planet_radius == other.planet_radius &&
				atmosphere_radius == other.atmosphere_radius;
		}

		bool operator!=(const AtmosphereProperties& other) const
		{
			return !((*this) == other);
		}
	};
}













#endif