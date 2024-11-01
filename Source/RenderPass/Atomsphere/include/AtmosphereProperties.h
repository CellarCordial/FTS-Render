#ifndef RENDER_PASS_ATMOSPHERE_PROPERTIES_H
#define RENDER_PASS_ATMOSPHERE_PROPERTIES_H


namespace FTS
{
	namespace Constant
	{
		struct AtmosphereProperties
		{
			FVector3F RayleighScatter = { 5.802f, 13.558f, 33.1f };     // (um^-1)
			FLOAT fRayleighDensity = 8.0f;   // (km), 大气中粒子的强度.

			FLOAT fMieScatter = 3.996f; // (um^-1)
			FLOAT fMieDensity = 1.2f;   // (km), 大气中粒子的强度.
			FLOAT fMieAbsorb = 4.4f;    // (um^-1)
			FLOAT fMieAsymmetry = 0.8f;

			FVector3F OzoneAbsorb = { 0.65f, 1.881f, 0.085f };  // (um^-1)
			FLOAT fOzoneCenterHeight = 25.0f;   // (km)

			FLOAT fOzoneThickness = 30.0f;      // (km)
			FLOAT fPlanetRadius = 6360.0f;      // (km)
			FLOAT fAtmosphereRadius = 6460.0f;  // (km)
			FLOAT PAD = 0.0f;

			AtmosphereProperties ToStandardUnit()
			{
				return AtmosphereProperties{
					.RayleighScatter = RayleighScatter * 1e-6f,
					.fRayleighDensity = fRayleighDensity * 1e3f,
					.fMieScatter = fMieScatter * 1e-6f,
					.fMieDensity = fMieDensity * 1e3f,
					.fMieAbsorb = fMieAbsorb * 1e-6f,
					.fMieAsymmetry = fMieAsymmetry,
					.OzoneAbsorb = OzoneAbsorb * 1e-6f,
					.fOzoneCenterHeight = fOzoneCenterHeight * 1e3f,
					.fOzoneThickness = fOzoneThickness * 1e3f,
					.fPlanetRadius = fPlanetRadius * 1e3f,
					.fAtmosphereRadius = fAtmosphereRadius * 1e3f
				};
			}

			BOOL operator==(const AtmosphereProperties& crOther) const
			{
				return 	RayleighScatter == crOther.RayleighScatter &&
					fRayleighDensity == crOther.fRayleighDensity &&
					fMieScatter == crOther.fMieScatter &&
					fMieDensity == crOther.fMieDensity &&
					fMieAbsorb == crOther.fMieAbsorb &&
					fMieAsymmetry == crOther.fMieAsymmetry &&
					OzoneAbsorb == crOther.OzoneAbsorb &&
					fOzoneCenterHeight == crOther.fOzoneCenterHeight &&
					fOzoneThickness == crOther.fOzoneThickness &&
					fPlanetRadius == crOther.fPlanetRadius &&
					fAtmosphereRadius == crOther.fAtmosphereRadius;
			}

			BOOL operator!=(const AtmosphereProperties& crOther) const
			{
				return !((*this) == crOther);
			}
		};
	};
}













#endif