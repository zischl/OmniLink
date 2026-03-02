
#pragma once
#include <Helper.h>

#include <iostream>



void VarienceTest()
{

	ArraySwapLayout test;

	FuncArgTypes payload{ test };

	unsigned char* Arg = reinterpret_cast<unsigned char*>(&payload);

	FuncArgTypes out;

	Variance::VariantDeserializer<FuncArgTypes>
		(
			out,
			0,
			std::make_index_sequence<std::variant_size_v<FuncArgTypes>>{},
			Arg
		);


	ArraySwapLayout args = std::get<0>(out);

	std::cout << args.index1 << " " << args.index2 << "\n";
}



