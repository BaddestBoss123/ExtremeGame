from PIL import Image, ImageColor

# # Load the original image
# original_image_path = "discord.png"
# image = Image.open(original_image_path)

# # Define the size of the new image
# new_width = image.width
# new_height = image.height + 12  # 6 rows above and 6 rows below
# new_size = (new_width, new_height)

# # Create a new blank image with the desired size and background color (rgba(0, 0, 0, 0))
# new_image = Image.new("RGBA", new_size, ImageColor.getcolor("rgba(0, 0, 0, 0)", "RGBA"))

# # Paste the original image onto the new image, leaving 6 rows above and 6 rows below
# new_image.paste(image, (0, 6))

# # Save the resulting image
# new_image.save("discord.png")



input_image_path = "multiplayer.png"
output_image_path = "multiplayer.png"

image = Image.open(input_image_path)

print(image.mode)

alpha_channel = image.split()[3]
red_channel = alpha_channel.copy()

new_image = Image.new("RGBA", image.size)
new_image.paste(red_channel, (0, 0), alpha_channel)
new_image.save(output_image_path)