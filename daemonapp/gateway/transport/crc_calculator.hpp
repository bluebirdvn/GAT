#ifndef _CRC_CALCULATOR_H
#define _CRC_CALCULATOR_H


class CrcCalculator {
	public:
	uint16_t calculate(const uint8_t *data, size_t len) const;
	bool verify (std::vector<uint8_t>& frame) const;
}
#endif //_CRC_CALCULATOR_H